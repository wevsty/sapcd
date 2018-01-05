#ifndef __ASIO_ASYNC_SMTP_SCANNER_H__
#define __ASIO_ASYNC_SMTP_SCANNER_H__

#include "asio_async_tcp_server_template.hpp"
#include "asio_async_tcp_client_template.hpp"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include "pystring_extend/pystring_class.h"
#include "smtp_protocol_class.hpp"
#include "spamd_protocol.hpp"
#include <string>
#include "ip_list_filter.h"
#include "data_size_list_filter.h"
#include "proc_limit.h"

//SMTP SESSION默认缓冲区大小
#define SMTP_SESSION_DEFAULT_BUFFER_SIZE (8138)
//SMTP默认超时时间
#define SMTP_SESSION_DEFAULT_TIMEOUT 60
//SPAMD SESSION默认缓冲区大小
#define SPAMD_SESSION_DEFAULT_BUFFER_SIZE (8138)

class smtp_client_session : public async_tcp_client
{
public:
	static int conf_smtp_client_timeout;
	static std::string conf_smtp_to_host;
	static std::string conf_smtp_to_port;
	static boost::asio::ip::tcp::endpoint conf_smtp_to_endpoint;
	//
	smtp_client_protocol_class smtp_client_protocol;
	//pystring_ext m_data_buffer;
	smtp_client_session(boost::asio::io_service &io_service, const smtp_info_class &input_info);
	virtual ~smtp_client_session();

	virtual void handle_protocol_connected();
	virtual void handle_protocol_read(boost::shared_ptr<char> data_buffer,
		size_t bytes_transferre);

	virtual void handle_protocol_write(boost::shared_ptr<std::string> pstr,
		std::size_t bytes_transferred);

	virtual void handle_protocol_closed();
};

class async_spamd_client : public async_tcp_client
{
public:

	static int conf_spamd_max_scanner_bytes;
	static int conf_spamd_timeout;
	static int conf_spamd_max_session;
	static std::string conf_spamd_host;
	static std::string conf_spamd_port;
	static boost::asio::ip::tcp::endpoint conf_spamd_endpoint;
	static proc_limit conf_spamd_limit;
	//
	enum {
		ASYNC_WAIT_SEND,
		ASYNC_WAIT_TIMEOUT,
		ASYNC_ALLOW_SEND
	};
	//bool b_exit;
	smtp_info_class smtp_info;
	pystring_ext m_pystr_report_data;
	boost::asio::deadline_timer spamd_wait_timer;
	//boost::mutex m_spamd_user_mutex;
	volatile int m_wait_status;
	//boost::shared_ptr<async_tcp_client> ptr_async_client;
	async_spamd_client(boost::asio::io_service &io_service, const smtp_info_class & info);

	void send_data_to_next_smtp_server();
	void auto_smart_send();
	int is_wait_send();
	int get_start_status();

	virtual void handle_protocol_connected();
	virtual void handle_protocol_read(boost::shared_ptr<char> data_buffer,
		size_t bytes_transferre);
	virtual void handle_protocol_write(boost::shared_ptr<std::string> pstr,
		std::size_t bytes_transferred);
    virtual void handle_protocol_timer_first_start();
	virtual void handle_protocol_timer(time_t last_trans_elapsed);
	virtual void handle_protocol_closed();

	boost::shared_ptr<async_spamd_client> get_shared_from_this();
};

//smtp server session
class smtp_scanner_server_session : public async_tcp_session
{
public:
	//静态变量
	static ip_list_filter conf_session_ip_filter;
	static data_size_list_filter conf_session_data_size_filter;
	static int conf_session_timeout;
	//
	smtp_server_protocol_class smtp_protocol;
	boost::asio::deadline_timer smtp_wait_timer;
	boost::mutex m_user_mutex;
	volatile bool m_b_exit;
	smtp_scanner_server_session(boost::asio::io_service &io_service);
	virtual ~smtp_scanner_server_session();

	virtual void handle_protocol_connected();
	virtual void handle_protocol_read(boost::shared_ptr<char> data_buffer,
		size_t bytes_transferre);
	virtual void handle_protocol_write(boost::shared_ptr<std::string> pstr,
		std::size_t bytes_transferred);
	virtual void handle_protocol_timer(time_t last_trans_elapsed);
	virtual void handle_protocol_closed();

	void wait_send_reply(boost::shared_ptr<async_spamd_client> spamd_client_ptr, std::string str_reply);
	void nowait_send_reply(const std::string &str_reply);

	boost::shared_ptr<smtp_scanner_server_session> get_shared_from_this();
};
#endif
