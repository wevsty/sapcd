#ifndef __ASIO_ASYNC_TCP_CLIENT_TEMPLATE_HPP__
#define __ASIO_ASYNC_TCP_CLIENT_TEMPLATE_HPP__
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

#include "std_timer_class.hpp"

//默认30秒超时时间，如果超过时间无数据则自动断开
#define ASYNC_CLIENT_DEFAULT_TIMEOUT_SEC 60
//检查超时的时间间隔
#define ASYNC_CLIENT_DEFAULT_TIMER_CHECK_TIME 1
//默认缓冲区大小
#define ASYNC_CLIENT_DEFAULT_BUFFER_BYTES_SIZE 4096

//async_tcp_client不能再栈上使用
class async_tcp_client :
	public boost::enable_shared_from_this<async_tcp_client>,
	private boost::noncopyable
{
public:
	async_tcp_client(boost::asio::io_service &io_service)
		: m_io_service(io_service),
		m_resolver(io_service),
		m_socket(io_service),
		m_io_mutex(), m_close_mutex(),
		m_timeouts(ASYNC_CLIENT_DEFAULT_TIMEOUT_SEC), m_last_action_time_sec(),
		m_io_timer(io_service), m_connect_timer(io_service),
		n_socket_status(SOCKET_STATUS_NORMAL),
		m_buffer_size(ASYNC_CLIENT_DEFAULT_BUFFER_BYTES_SIZE)
	{

	}
	virtual ~async_tcp_client()
	{
		//虚析构函数
		//std::cout << "delete tcp session" << std::endl;
	}

	boost::asio::io_service &get_io_service()
	{
		return m_io_service;
	}

	boost::asio::ip::tcp::socket &socket()
	{
		return m_socket;
	}

	static boost::asio::ip::tcp::endpoint reslove_endpoint(const std::string host, const std::string port)
	{
		boost::asio::io_service reslove_io_service;
		boost::asio::ip::tcp::resolver tmp_resolver(reslove_io_service);
		boost::asio::ip::tcp::resolver::iterator it =
			tmp_resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
		return *it;
	}
	
	void async_connect_server(
		const std::string host, const std::string port,
		int timeouts = ASYNC_CLIENT_DEFAULT_TIMEOUT_SEC
	)
	{
		m_timeouts = timeouts;
		boost::asio::ip::tcp::resolver::iterator it =
			m_resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
		async_start_connect(it);
	}

	void async_connect_server(boost::asio::ip::tcp::endpoint input_endpoint,
		int timeouts = ASYNC_CLIENT_DEFAULT_TIMEOUT_SEC)
	{
		//	boost::asio::ip::tcp::resolver::iterator it =
		//	m_resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
		//	it->endpoint()
		m_timeouts = timeouts;
		async_start_connect(input_endpoint);
	}


	void async_recv_data()
	{
		//申请缓冲区
		boost::shared_ptr<char> data_buffer(make_safe_buffer(m_buffer_size, true));

		/*
		boost::shared_ptr<char> data_buffer(
		new char[m_buffer_size],
		std::default_delete<char[]>()
		);
		memset(data_buffer.get(), 0, m_buffer_size);
		*/
		m_last_action_time_sec.restart();
		m_socket.async_read_some(boost::asio::buffer(data_buffer.get(),
			m_buffer_size),
			boost::bind(&async_tcp_client::handle_tcp_read, shared_from_this(),
				boost::asio::placeholders::error,
				data_buffer,
				boost::asio::placeholders::bytes_transferred)
		);
	}

	void async_send_data(const std::string str_data)
	{
		boost::shared_ptr<std::string> pstr(new std::string(str_data));
		//*pstr = str_data;
		m_last_action_time_sec.restart();
		
		m_socket.async_write_some(boost::asio::buffer(*pstr),
			boost::bind(&async_tcp_client::handle_tcp_write, shared_from_this(),
				boost::asio::placeholders::error,
				pstr,
				boost::asio::placeholders::bytes_transferred
			)
		);
		/*
		boost::asio::async_write(m_socket,boost::asio::buffer(*pstr),
			boost::bind(&async_tcp_client::handle_tcp_write, shared_from_this(),
				boost::asio::placeholders::error,
				pstr,
				boost::asio::placeholders::bytes_transferred
			)
		);
		*/
	}

	void close_tcp_session()
	{
		//确保只有一个close函数在运行
		//Linux下多个close函数一起执行会导致程序崩溃，而Windows不会？
		boost::mutex::scoped_lock lk(m_close_mutex);
		if (n_socket_status != SOCKET_STATUS_CLOSED)
		{
			n_socket_status = SOCKET_STATUS_CLOSED;
			close_socket(m_socket);
			all_timer_cancel();
			handle_protocol_closed();
		}
	}
	virtual void handle_protocol_connected()
	{
		//留给用户实现协议
		async_send_data("Client Hello World\n");
		return;
	}

	virtual void handle_protocol_read(boost::shared_ptr<char> data_buffer,
		size_t bytes_transferre)
	{
		//留给用户实现协议
		async_send_data("Client Say Hello\n");
		return;
	}

	virtual void handle_protocol_write(boost::shared_ptr<std::string> pstr,
		std::size_t bytes_transferred)
	{
		//留给用户实现协议
		//默认发送成功后就异步接收
		async_recv_data();
		return;
	}

	virtual void handle_protocol_timer_first_start()
	{
		io_timer_call_at(ASYNC_CLIENT_DEFAULT_TIMER_CHECK_TIME);
	}

	virtual void handle_protocol_timer(time_t last_trans_elapsed)
	{
		if (last_trans_elapsed > ASYNC_CLIENT_DEFAULT_TIMEOUT_SEC)
		{
			close_tcp_session();
			return;
		}
		io_timer_call_at(ASYNC_CLIENT_DEFAULT_TIMER_CHECK_TIME);
		return;
	}

	virtual void handle_protocol_closed()
	{
		//本函数内调用close_tcp_session()会导致死锁。
		//如果派生类中没有需要主动close的资源，请不要重写此函数。
		return;
	}

protected:

	void set_no_delay()
	{
		try
		{
			boost::asio::ip::tcp::no_delay noDelayOption(true);
			m_socket.set_option(noDelayOption);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception: " << e.what() << "\n";
		}
	}

	void set_last_action_time()
	{
		m_last_action_time_sec.restart();
	}

	//获取上次操作的时间
	time_t get_last_trans_elapsed()
	{
		return m_last_action_time_sec.elapsed();
	}

	//n秒后调用handle_io_timer回调函数
	void io_timer_call_at(const int n_second)
	{
		m_io_timer.expires_from_now(boost::posix_time::seconds(
			n_second));
		m_io_timer.async_wait(
			boost::bind(&async_tcp_client::handle_io_timer,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}

	void io_timer_call_at_infin(const int n_second)
	{
		m_io_timer.expires_from_now(boost::posix_time::pos_infin);
	}

	//n秒后调用handle_connect_timer回调函数
	void connect_timer_call_at(const int n_second)
	{
		m_connect_timer.expires_from_now(boost::posix_time::seconds(
			n_second));
		m_connect_timer.async_wait(
			boost::bind(&async_tcp_client::handle_connect_timer,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}
	void connect_timer_call_at_infin(const int n_second)
	{
		m_connect_timer.expires_from_now(boost::posix_time::pos_infin);
	}

	void set_buffer_size(const int new_buffer_size)
	{
		m_buffer_size = new_buffer_size;
	}
	/*
	void set_default_timeouts(int timeouts)
	{
	m_timeouts = timeouts;
	}
	*/
private:
	void async_start_connect(boost::asio::ip::tcp::endpoint endpoint)
	{
		/*
		std::cout << "Trying connect "
			<< endpoint
			<< " ..." << std::endl;
		*/
		//记录开始时间
		m_last_action_time_sec.restart();
		//timeout check
		connect_timer_call_at(m_timeouts);
		// Start the asynchronous connect operation.
		m_socket.async_connect(endpoint,
			boost::bind(&async_tcp_client::handle_tcp_once_connect,
				shared_from_this(),
				boost::asio::placeholders::error)
		);
	}

	void async_start_connect(boost::asio::ip::tcp::resolver::iterator endpoint_iter)
	{
		if (endpoint_iter != boost::asio::ip::tcp::resolver::iterator())
		{
			/*
			std::cout << "Trying connect "
				<< endpoint_iter->endpoint()
				<< " ..." << std::endl;
			*/
			//记录开始时间
			m_last_action_time_sec.restart();
			//timeout check
			connect_timer_call_at(m_timeouts);
			// Start the asynchronous connect operation.
			m_socket.async_connect(endpoint_iter->endpoint(),
				boost::bind(&async_tcp_client::handle_tcp_iterator_connect,
					shared_from_this(),
					boost::asio::placeholders::error,
					endpoint_iter)
			);
		}
		else
		{
			//std::cout << "No more endpoint. Shutdown Client" << std::endl;
			// There are no more endpoints to try. Shut down the client.
			close_tcp_session();
		}
	}

	//取消所有定时器所有任务
	void all_timer_cancel()
	{
		m_io_timer.cancel();
		m_connect_timer.cancel();
	}

	//关闭socket
	void close_socket(boost::asio::ip::tcp::socket &close_socket)
	{
		if (close_socket.is_open() == true)
		{
			boost::system::error_code ec;
			close_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			ec.clear();
			close_socket.close(ec);
			ec.clear();
		}
	}

	//io timer回调函数
	void handle_io_timer(const boost::system::error_code &error)
	{
		boost::mutex::scoped_lock lk(m_io_mutex);
		if (n_socket_status != SOCKET_STATUS_CLOSED)
		{
			time_t last_trans_elapsed = m_last_action_time_sec.elapsed();
			handle_protocol_timer(last_trans_elapsed);
		}
	}

	//connect timer回调函数
	void handle_connect_timer(const boost::system::error_code &error)
	{
		boost::mutex::scoped_lock lk(m_io_mutex);
		if (m_socket.is_open() == true)
		{
			return;
		}
		else
		{
			close_tcp_session();
		}
	}

	void handle_tcp_once_connect(const boost::system::error_code &error)
	{
		//确保回调函数不会冲突
		boost::mutex::scoped_lock lk(m_io_mutex);
		if (error)
		{
			//std::cout << "Connect failed: " << error.message() << std::endl;
			// Check if the connect operation failed before the deadline expired.
			//如果出现错误则close
			close_tcp_session();
		}
		else
		{
			//成功连接
			n_socket_status = SOCKET_STATUS_NORMAL;
			m_last_action_time_sec.restart();
			//timer task first start
			handle_protocol_timer_first_start();
			//调用用户函数
			handle_protocol_connected();
		}
	}

	void handle_tcp_iterator_connect(const boost::system::error_code &error,
		boost::asio::ip::tcp::resolver::iterator endpoint_iter
	)
	{
		//确保回调函数不会冲突
		boost::mutex::scoped_lock lk(m_io_mutex);
		if (n_socket_status == SOCKET_STATUS_CLOSED)
		{
			//std::cout << "Connect timed out" << std::endl;
			return;
		}
		if (m_socket.is_open() == false)
		{
			//std::cout << "Connect failed. Try next endpoint" << std::endl;
			// Try the next available endpoint.
			async_start_connect(++endpoint_iter);
		}
		else if (error)
		{
			//std::cout << "Connect error: " << error.message() << std::endl;
			// Check if the connect operation failed before the deadline expired.
			//如果出现错误则close
			close_tcp_session();
		}
		else
		{
			//成功连接
			n_socket_status = SOCKET_STATUS_NORMAL;
			m_last_action_time_sec.restart();
			handle_protocol_connected();
			handle_protocol_timer_first_start();
		}
	}

	//async_recv回调函数
	void handle_tcp_read(const boost::system::error_code &error,
		boost::shared_ptr<char> data_buffer, size_t bytes_transferre)
	{
		//确保回调函数不会冲突
		boost::mutex::scoped_lock lk(m_io_mutex);
		m_last_action_time_sec.restart();
		if (!error)
		{
			handle_protocol_read(data_buffer, bytes_transferre);
		}
		else
		{
			//std::cout << error << std::endl;
			//如果出现错误则close
			close_tcp_session();
		}
	}

	//async_sender回调函数
	void handle_tcp_write(const boost::system::error_code &error
		, boost::shared_ptr<std::string> pstr, std::size_t bytes_transferred)
	{
		//确保回调函数不会冲突
		boost::mutex::scoped_lock lk(m_io_mutex);
		m_last_action_time_sec.restart();
		if (!error)
		{
			handle_protocol_write(pstr, bytes_transferred);
		}
		else
		{
			//如果出现错误则close
			close_tcp_session();
		}
	}

	boost::shared_ptr<char> make_safe_buffer(size_t size,
		bool b_fill_zero_to_memory = true)
	{
		//default_delete是STL中的默认删除器
		size_t safe_size = size + 1;
		boost::shared_ptr<char> ret_ptr(
			new char[safe_size],
			std::default_delete<char[]>()
		);
		if (b_fill_zero_to_memory == true)
		{
			memset(ret_ptr.get(), 0, safe_size * sizeof(char));
		}
		return ret_ptr;
	}

	boost::asio::io_service &m_io_service;
	boost::asio::ip::tcp::resolver m_resolver;
	boost::asio::ip::tcp::socket m_socket;
	boost::mutex m_io_mutex;
	boost::mutex m_close_mutex;
	int m_timeouts;
	TIMER_SEC m_last_action_time_sec;
	boost::asio::deadline_timer m_io_timer;
	boost::asio::deadline_timer m_connect_timer;
	//enum { max_length = 1024 };
	//char data_[max_length];
	enum
	{
		SOCKET_STATUS_NORMAL = 0,
		SOCKET_STATUS_CLOSED = 1
	};
	volatile int n_socket_status;
	int m_buffer_size;
};
#endif
