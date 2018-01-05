#include "asio_async_tcp_server_template.hpp"
#include "asio_async_tcp_client_template.hpp"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include "pystring_extend/pystring_class.h"
#include "smtp_protocol_class.hpp"
#include "spamd_protocol.hpp"
#include <string>
#include "async_smtp_scanner.h"
#include "ip_list_filter.h"
#define WAIT_SEND_CHECK_TIME_MS (30)
//smtp server config
ip_list_filter smtp_scanner_server_session::conf_session_ip_filter;
data_size_list_filter smtp_scanner_server_session::conf_session_data_size_filter;
int smtp_scanner_server_session::conf_session_timeout = 60;

//spamd config
std::string async_spamd_client::conf_spamd_host = "127.0.0.1";
std::string async_spamd_client::conf_spamd_port = "783";
boost::asio::ip::tcp::endpoint async_spamd_client::conf_spamd_endpoint;
int async_spamd_client::conf_spamd_timeout = 60;
int async_spamd_client::conf_spamd_max_scanner_bytes = 2048 * 1024;
int async_spamd_client::conf_spamd_max_session = 64;
proc_limit async_spamd_client::conf_spamd_limit;
//smtp client config
std::string smtp_client_session::conf_smtp_to_host = "127.0.0.1";
std::string smtp_client_session::conf_smtp_to_port = "10026";
boost::asio::ip::tcp::endpoint smtp_client_session::conf_smtp_to_endpoint;
int smtp_client_session::conf_smtp_client_timeout = 60;

//SMTP SESSION默认缓冲区大小
#define SMTP_SESSION_DEFAULT_BUFFER_SIZE (8138)
//SMTP默认超时时间
#define SMTP_SESSION_DEFAULT_TIMEOUT 60
//SPAMD SESSION默认缓冲区大小
#define SPAMD_SESSION_DEFAULT_BUFFER_SIZE (8138)

void debug_print(const std::string &debug_str)
{
#ifdef _DEBUG
	std::cout << debug_str << std::endl;
#endif
}


smtp_client_session::smtp_client_session(boost::asio::io_service &io_service,
	const smtp_info_class &input_info)
	: async_tcp_client(io_service), smtp_client_protocol(input_info)
{
	//改变默认的缓冲区设置
	set_buffer_size(SMTP_SESSION_DEFAULT_BUFFER_SIZE);
}

smtp_client_session::~smtp_client_session()
{
	//async_tcp_session::~async_tcp_session();
	//std::cout << "delete smtp session" << std::endl;
	debug_print("delete smtp client session");
}

void smtp_client_session::handle_protocol_connected()
{
	async_recv_data();
}

void smtp_client_session::handle_protocol_read(boost::shared_ptr<char>
	data_buffer,
	size_t bytes_transferre)
{
	pystring_ext str_data(data_buffer.get(), bytes_transferre);
	//m_data_buffer.append(str_data);
	if (smtp_client_protocol.is_command_line(str_data) == false)
	{
		async_recv_data();
		return;
	}
	pystring_ext pystr_send = smtp_client_protocol.from_revc_get_next_data(
		str_data);
	if (pystr_send.empty() == true)
	{
		close_tcp_session();
	}
	else
	{
		async_send_data((const std::string)pystr_send);
	}
	return;
}

void smtp_client_session::handle_protocol_write(boost::shared_ptr<std::string>
	pstr,
	std::size_t bytes_transferred)
{
	async_recv_data();
	return;
}

void smtp_client_session::handle_protocol_closed()
{
	debug_print("smtp client disconnect");
	return;
}

//async_spamd_client
async_spamd_client::async_spamd_client(boost::asio::io_service &io_service,
	const smtp_info_class &info)
	: async_tcp_client(io_service),
	smtp_info(info),
	m_pystr_report_data(),
	spamd_wait_timer(io_service),
	m_wait_status(ASYNC_WAIT_SEND)
{
	//改变默认的缓冲区设置
	set_buffer_size(SPAMD_SESSION_DEFAULT_BUFFER_SIZE);
	set_last_action_time();
}

void async_spamd_client::handle_protocol_connected()
{
	set_no_delay();
	spamd_request spamd_req;
	pystring_ext str_req = spamd_req.spamd_process(smtp_info.session_data);
	async_send_data((const std::string)str_req);
	debug_print("send to spamd");
	//spamd_wait_timer.expires_from_now(boost::posix_time::milliseconds(2000));
	//spamd_wait_timer.wait();
	//std::cout << "connected spamd" << std::endl;
	//std::cout << "limit : "<< conf_spamd_limit.get_number() << std::endl;
	//std::cout << conf_spamd_limit. << std::endl;
	return;
}

void async_spamd_client::send_data_to_next_smtp_server()
{
	//smtp 发送数据
	boost::shared_ptr<smtp_client_session> p_send(
		new smtp_client_session(get_io_service(), smtp_info)
	);
	p_send->async_connect_server(
		smtp_client_session::conf_smtp_to_endpoint,
		smtp_client_session::conf_smtp_client_timeout
	);
}

int async_spamd_client::is_wait_send()
{
		if (conf_spamd_limit.is_need_wait(conf_spamd_max_session) == true)
		{
			return ASYNC_WAIT_SEND;
		}
		else
		{
			//允许发送
			return ASYNC_ALLOW_SEND;
		}
		if (get_last_trans_elapsed() > async_spamd_client::conf_spamd_timeout)
		{
			//等待时间过长，抛弃
			return ASYNC_WAIT_TIMEOUT;
		}
}
int async_spamd_client::get_start_status()
{
	//if (m_wait_status == ASYNC_WAIT_SEND)
	return m_wait_status;
}
void async_spamd_client::auto_smart_send()
{
	
	if (smtp_info.session_data.size() >= conf_spamd_max_scanner_bytes)
	{
		send_data_to_next_smtp_server();
		return;
	}
	m_wait_status = is_wait_send();
	if (m_wait_status == ASYNC_WAIT_SEND)
	{
		spamd_wait_timer.expires_from_now(boost::posix_time::milliseconds(WAIT_SEND_CHECK_TIME_MS));
		spamd_wait_timer.async_wait(
			boost::bind(
				&async_spamd_client::auto_smart_send,
				get_shared_from_this()
				)
		);
		return;
	}
	else if (m_wait_status == ASYNC_WAIT_TIMEOUT)
	{
		return;
	}
	else
	{
		async_connect_server(
			async_spamd_client::conf_spamd_endpoint,
			async_spamd_client::conf_spamd_timeout
		);
	}
}

void async_spamd_client::handle_protocol_read(boost::shared_ptr<char>
	data_buffer,
	size_t bytes_transferre)
{
	pystring_ext pystr_data((const char *)data_buffer.get(), bytes_transferre);
	m_pystr_report_data.append(pystr_data);
	//收到spamd回复之后
	spamd_report spamd_rep;
	int n_spamd_status = spamd_rep.parse_report(m_pystr_report_data);
	if (n_spamd_status == spamd_report::SPAMD_HEADER_ERROR)
	{
		close_tcp_session();
	}
	else if (n_spamd_status == spamd_report::SPAMD_REPORT_ERROR)
	{
		close_tcp_session();
	}
	else if (n_spamd_status == spamd_report::SPAMD_DATALEN_ERROR)
	{
		async_recv_data();
	}
	else
	{
		m_pystr_report_data.clear();
		debug_print("recv from spamd");
		//更新数据为spamd分析后的数据
		smtp_info.session_data = spamd_rep.return_body_data();
		send_data_to_next_smtp_server();
		close_tcp_session();
	}
	return;
}

void async_spamd_client::handle_protocol_write(boost::shared_ptr<std::string>
	pstr,
	std::size_t bytes_transferred)
{
	//默认发送成功后就异步接收
	async_recv_data();
	return;
}

void async_spamd_client::handle_protocol_timer_first_start()
{
	io_timer_call_at(ASYNC_CLIENT_DEFAULT_TIMER_CHECK_TIME);
}

void async_spamd_client::handle_protocol_timer(time_t last_trans_elapsed)
{
	if (last_trans_elapsed > conf_spamd_timeout)
	{
	    //std::cout << "disconnected spamd by timer" << std::endl;
		close_tcp_session();
		return;
	}
	io_timer_call_at(ASYNC_CLIENT_DEFAULT_TIMER_CHECK_TIME);
	return;
}

void async_spamd_client::handle_protocol_closed()
{
	//本函数内调用close_tcp_session()会导致死锁。
	//如果派生类中没有需要主动close的资源，请不要重写此函数。
	conf_spamd_limit.DEC();
	spamd_wait_timer.cancel();
	//std::cout << "disconnected spamd" << std::endl;
	return;
}

boost::shared_ptr<async_spamd_client> async_spamd_client::get_shared_from_this()
{
	return boost::dynamic_pointer_cast<async_spamd_client>(shared_from_this());
}

//smtp_scanner_server_session
smtp_scanner_server_session::smtp_scanner_server_session(
	boost::asio::io_service &io_service)
	: async_tcp_session(io_service), smtp_protocol(),
	smtp_wait_timer(io_service), m_user_mutex(), m_b_exit(false)
{
	//改变默认的缓冲区设置
	set_buffer_size(SMTP_SESSION_DEFAULT_BUFFER_SIZE);
}

smtp_scanner_server_session::~smtp_scanner_server_session()
{
	//async_tcp_session::~async_tcp_session();
	//std::cout << "delete smtp session" << std::endl;
	debug_print("delete smtp server session");
}

void smtp_scanner_server_session::handle_protocol_connected()
{
	async_send_data("220 Welcome SAPCD\r\n");
}

boost::shared_ptr<smtp_scanner_server_session> smtp_scanner_server_session::get_shared_from_this()
{
	return boost::dynamic_pointer_cast<smtp_scanner_server_session>(shared_from_this());
}
void smtp_scanner_server_session::wait_send_reply(
	boost::shared_ptr<async_spamd_client> spamd_client_ptr,
	std::string str_reply
)
{
	//确保函数不会冲突
	boost::mutex::scoped_lock lk(m_user_mutex);
	if (m_b_exit==true)
	{
		return;
	}
	volatile bool b_wait = true;
	int n_wait_status = spamd_client_ptr->get_start_status();
	if (n_wait_status == async_spamd_client::ASYNC_WAIT_SEND)
	{
		b_wait = true;
	}
	else
	{
		b_wait = false;
	}
	if (b_wait == true)
	{
		smtp_wait_timer.expires_from_now(boost::posix_time::milliseconds(WAIT_SEND_CHECK_TIME_MS));
		smtp_wait_timer.async_wait(
			boost::bind(
			&smtp_scanner_server_session::wait_send_reply,
			get_shared_from_this(),
			spamd_client_ptr,
			str_reply
			)
		);
		return;
	}

	//发送回复
	if (str_reply.size() != 0)
	{
		async_send_data(str_reply);
	}
	else
	{
		async_recv_data();
	}
}

void smtp_scanner_server_session::nowait_send_reply(const std::string &str_reply)
{
	if (str_reply.size() != 0)
	{
		async_send_data(str_reply);
	}
	else
	{
		async_recv_data();
	}
}

void smtp_scanner_server_session::handle_protocol_read(boost::shared_ptr<char>
	data_buffer,
	size_t bytes_transferre)
{
	pystring_ext str_data(data_buffer.get(), bytes_transferre);
	pystring_ext str_ret = smtp_protocol.read_raw_data(str_data);
	if (smtp_protocol.is_data_end() == true)
	{
		bool b_discard = false;
		pystring_ext pystr_ip = smtp_protocol.find_last_sender_ip();
		if (pystr_ip.empty() != true)
		{
			//IP添加到邮件头
			smtp_protocol.add_last_sender_ip_to_data_header(pystr_ip);
			//判断IP是否需要过滤
			b_discard = conf_session_ip_filter.is_will_discard(pystr_ip);
			conf_session_ip_filter.push(pystr_ip);
			//判断大小是否需要过滤
			size_t n_data_size = smtp_protocol.smtp_info.session_data.size();
			b_discard |= conf_session_data_size_filter.is_will_discard(n_data_size);
			conf_session_data_size_filter.push(n_data_size);
		}
		if (b_discard == true)
		{
			nowait_send_reply(str_ret);
			return;
		}
		//async_tcp_client模板是不能创建在栈上的必须new
		boost::shared_ptr<async_spamd_client> spamd_client_ptr(
			new async_spamd_client(get_io_service(), smtp_protocol.smtp_info)
		);
		//智能开始自动进行发送
		spamd_client_ptr->auto_smart_send();
		//等待spamd开始
		wait_send_reply(spamd_client_ptr, str_ret);
		return;
	}
	//发送回复
	nowait_send_reply(str_ret);
	return;
}

void smtp_scanner_server_session::handle_protocol_write(
	boost::shared_ptr<std::string> pstr,
	std::size_t bytes_transferred)
{
	//默认发送成功后就异步接收,如果协议已经发送过quit，则关闭session
	if (smtp_protocol.is_protocol_close() == true)
	{
		close_tcp_session();
	}
	else
	{
		async_recv_data();
	}
	return;
}

void smtp_scanner_server_session::handle_protocol_timer(
	time_t last_trans_elapsed)
{
	if (last_trans_elapsed > conf_session_timeout)
	{
		close_tcp_session();
		return;
	}
	io_timer_call_at(SERVER_DEFAULT_TIMER_CHECK_TIME);
	return;
}

void smtp_scanner_server_session::handle_protocol_closed()
{
	debug_print("smtp server session disconnect");
	m_b_exit = true;
	smtp_wait_timer.cancel();
	return;
}
