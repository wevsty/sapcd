#ifndef __ASIO_ASYNC_TCP_SERVER_TEMPLATE_HPP__
#define __ASIO_ASYNC_TCP_SERVER_TEMPLATE_HPP__

#include <iostream>
#include <string>
#include <vector>
//using namespace std;
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "std_timer_class.hpp"

//默认30秒超时时间，如果超过时间无数据则自动断开
#define SERVER_DEFAULT_TIMEOUT_SEC 60
//检查超时的时间间隔
#define SERVER_DEFAULT_TIMER_CHECK_TIME 1
//默认缓冲区大小
#define SERVER_DEFAULT_BUFFER_BYTES_SIZE 4096


class async_tcp_session :
	public boost::enable_shared_from_this<async_tcp_session>,
	private boost::noncopyable
{
public:
	async_tcp_session(boost::asio::io_service &io_service)
		: m_io_service(io_service),
		m_socket(io_service),
		m_io_mutex(), m_close_mutex(),
		m_last_action_time_sec(),
		m_io_timer(io_service),
		n_socket_status(SOCKET_STATUS_NORMAL),
		m_buffer_size(SERVER_DEFAULT_BUFFER_BYTES_SIZE)
	{
	}
	virtual ~async_tcp_session()
	{
		//虚析构函数
		//std::cout << "delete tcp session" << std::endl;
	}

	boost::asio::io_service & get_io_service()
	{
		return m_io_service;
	}

	boost::asio::ip::tcp::socket &socket()
	{
		return m_socket;
	}

	void start()
	{
		m_last_action_time_sec.restart();
		all_timer_cancel();
		//timer task first start
		handle_protocol_timer_first_start();
		//调用用户函数
		handle_protocol_connected();
	}

	void async_recv_data()
	{
		//申请缓冲区
		boost::shared_ptr<char> data_buffer(make_safe_buffer(m_buffer_size,true));
		
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
			boost::bind(&async_tcp_session::handle_tcp_read, shared_from_this(),
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
			boost::bind(&async_tcp_session::handle_tcp_write, shared_from_this(),
				boost::asio::placeholders::error,
				pstr,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}

	void close_tcp_session()
	{
		//确保只有一个close函数在运行
		//Linux下多个close函数一起执行会导致程序崩溃，而Windows不会？
		boost::mutex::scoped_lock lk(m_close_mutex);
		if (n_socket_status != SOCKET_STATUS_CLOSED)
		{
			n_socket_status = SOCKET_STATUS_CLOSED;
			all_timer_cancel();
			close_socket(m_socket);
			handle_protocol_closed();
		}
	}

	virtual void handle_protocol_connected()
	{
		//留给用户实现协议
		async_send_data("Server Hello World\n");
		return;
	}

	virtual void handle_protocol_read(boost::shared_ptr<char> data_buffer,
		size_t bytes_transferre)
	{
		//留给用户实现协议
		async_send_data("Server Say Hello\n");
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
		io_timer_call_at(SERVER_DEFAULT_TIMER_CHECK_TIME);
	}

	virtual void handle_protocol_timer(time_t last_trans_elapsed)
	{
		if (last_trans_elapsed > SERVER_DEFAULT_TIMEOUT_SEC)
		{
			close_tcp_session();
			return;
		}
		io_timer_call_at(SERVER_DEFAULT_TIMER_CHECK_TIME);
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
			boost::bind(&async_tcp_session::handle_io_timer,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}
	//取消io_timer_回调
	void io_timer_call_cancel(const int n_second)
	{
		m_io_timer.cancel();
	}

	void io_timer_call_at_infin(const int n_second)
	{
		m_io_timer.expires_from_now(boost::posix_time::pos_infin);
	}

	void set_buffer_size(const int new_buffer_size)
	{
		m_buffer_size = new_buffer_size;
	}

private:
	//取消所有定时器任务
	void all_timer_cancel()
	{
		m_io_timer.cancel();
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

	//timer回调函数
	void handle_io_timer(const boost::system::error_code &error)
	{
		boost::mutex::scoped_lock lk(m_io_mutex);
		if (n_socket_status != SOCKET_STATUS_CLOSED)
		{
			time_t last_action_elapsed = m_last_action_time_sec.elapsed();
			handle_protocol_timer(last_action_elapsed);
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
	boost::asio::ip::tcp::socket m_socket;
	boost::mutex m_io_mutex;
	boost::mutex m_close_mutex;
	//int m_timeouts;
	TIMER_SEC m_last_action_time_sec;
	boost::asio::deadline_timer m_io_timer;
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

template <class session_class>
class async_tcp_server : private boost::noncopyable
{
private:
	boost::asio::io_service &m_io_service;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::size_t n_thread_pool_size;
	std::vector<boost::shared_ptr<boost::thread> > m_vec_pthreads;
	boost::asio::ip::tcp::endpoint bind_endpoint;

public:
	boost::asio::ip::tcp::endpoint resolve_string(const std::string &address,
		const std::string &port)
	{
		boost::asio::ip::tcp::resolver resolver(m_io_service);
		boost::asio::ip::tcp::resolver::query query(address, port);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
		return endpoint;
	}

	async_tcp_server(
		boost::asio::io_service &io_service,
		std::size_t thread_pool_size,
		const std::string &address, const std::string &port
	)
		: m_io_service(io_service),
		m_acceptor(io_service),
		n_thread_pool_size(thread_pool_size),
		m_vec_pthreads(),
		bind_endpoint()
	{
		bind_endpoint = resolve_string(address, port);
		m_acceptor.open(bind_endpoint.protocol());
		m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		m_acceptor.bind(bind_endpoint);
		m_acceptor.listen();
		start_accept();
	}

	void run()
	{
		// Create a pool of threads to run all of the io_services.
		for (std::size_t i = 0; i < n_thread_pool_size; ++i)
		{
			boost::shared_ptr<boost::thread> thread(new boost::thread(
				boost::bind(&boost::asio::io_service::run, &m_io_service)));
			m_vec_pthreads.push_back(thread);
		}
	}

	void wait()
	{
		// Wait for all threads in the pool to exit.
		for (std::size_t i = 0; i < m_vec_pthreads.size(); ++i)
		{
			m_vec_pthreads[i]->join();
		}
	}

	void start_accept()
	{
		/*
		boost::shared_ptr<session> new_session(session::new_smart_session(m_io_service));
		m_acceptor.async_accept(new_session->socket(),
		boost::bind(&server::handle_accept, this, new_session,
		boost::asio::placeholders::error));
		*/
		//session_class *new_session = new session_class(m_io_service);
		boost::shared_ptr<session_class> new_session(
			new session_class(m_io_service)
		);
		m_acceptor.async_accept(new_session->socket(),
			boost::bind(&async_tcp_server::handle_accept, this, new_session,
				boost::asio::placeholders::error));
	}

	void handle_accept(boost::shared_ptr<session_class> new_session,
		const boost::system::error_code &error)
	{
		/*
		if (!error)
		{
		new_session->start(realy_endpoint, spamd_endpoint, n_timeouts,
		n_max_filter_size);
		}
		start_accept();
		*/
		if (!error)
		{
			new_session->start();
		}
		/*
		else
		{
		delete new_session;
		}
		*/
		start_accept();
	}

	void handle_stop()
	{
		m_io_service.stop();
	}

};

#endif // !__ASIO_ASYNC_TCP_SERVER_TEMPLATE_HPP__
