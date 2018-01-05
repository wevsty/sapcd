#ifndef __SMTP_PROTOCOL_CLASS_HPP__
#define __SMTP_PROTOCOL_CLASS_HPP__
#include <vector>
#include <string>
#include "pystring_extend/pystring_class.h"

class smtp_info_class
{
public:
	pystring_ext session_command_helo;
	pystring_ext session_command_ehlo;
	pystring_ext session_command_mail_from;
	std::vector<pystring_ext> session_command_rcpt_to;
	pystring_ext session_data;
	smtp_info_class()
	{

	}
    smtp_info_class(const smtp_info_class& input_info)
    :
        session_command_helo(input_info.session_command_helo),
        session_command_ehlo(input_info.session_command_ehlo),
        session_command_mail_from(input_info.session_command_mail_from),
        session_command_rcpt_to(input_info.session_command_rcpt_to),
        session_data(input_info.session_data)
    {

    }
	void clear()
	{
		session_command_helo.clear();
		session_command_ehlo.clear();
		session_command_mail_from.clear();
		session_command_rcpt_to.clear();
		session_data.clear();
	}
};

class smtp_client_protocol_class
{
public:
	smtp_info_class smtp_info;
	enum
	{
		//SEND_SETP_CONNECTED = 1,
		SEND_SETP_EHLO,
		SEND_SETP_MAILFROM,
		SEND_SETP_RCPTTO,
		SEND_SETP_DATA,
		SEND_SETP_DATAEND,
		SEND_SETP_WAIT_DATA_END,
		SEND_SETP_WAIT_QUIT,
		SEND_SETP_QUIT
	};
	int n_send_setp;
	enum
	{
		STATUS_NORMAL,
		STATUS_ERROR
	};
	int n_session_status;
	smtp_client_protocol_class(smtp_info_class input_info)
		: smtp_info(input_info), n_send_setp(SEND_SETP_EHLO),
		n_session_status(STATUS_NORMAL)
	{
	}

	bool is_can_start_ehlo(int status_code)
	{
		//smtp 状态码 220为正常
		//220 filter.local ESMTP Postfix (Ubuntu)
		if (status_code == 220)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}

	pystring_ext send_cmd_ehlo()
	{
		n_send_setp = SEND_SETP_MAILFROM;
		return "ehlo filter.local\r\n";
	}

	bool is_can_start_mail_from(int status_code)
	{
		//smtp 状态码 250为正常
		//250-filter.local
		//250 2.1.0 Ok
		if (status_code == 250)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}
	pystring_ext send_cmd_mail_from()
	{
		n_send_setp = SEND_SETP_RCPTTO;
		pystring_ext pystr_mailfrom = "mail FROM:<%s>\r\n";
		pystr_mailfrom = pystr_mailfrom.py_replace("%s",
			smtp_info.session_command_mail_from);
		return pystr_mailfrom;
	}

	bool is_can_start_rcpt_to(int status_code)
	{
		//smtp 状态码 250为正常
		//250 2.1.0 Ok
		if (status_code == 250)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}

	pystring_ext send_cmd_rcpt_to()
	{
		n_send_setp = SEND_SETP_DATA;
		pystring_ext pystr_rcpt = "rcpt TO:<%s>\r\n";
		if (smtp_info.session_command_rcpt_to.size() == 0)
		{
			n_session_status = STATUS_ERROR;
			return "";
		}
		pystr_rcpt = pystr_rcpt.py_replace("%s", smtp_info.session_command_rcpt_to[0]);
		return pystr_rcpt;
	}

	bool is_can_start_data(int status_code)
	{
		//smtp 状态码 250为正常
		//250 2.1.0 Ok
		if (status_code == 250)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}

	pystring_ext send_cmd_data_start()
	{
		n_send_setp = SEND_SETP_DATAEND;
		return "data\r\n";
	}

	bool is_can_start_data_end(int status_code)
	{
		//smtp 状态码 354为正常
		//354 End data with <CR><LF>.<CR><LF>
		if (status_code == 354)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}

	pystring_ext send_cmd_data_end()
	{
		n_send_setp = SEND_SETP_WAIT_DATA_END;
		pystring_ext send_data = smtp_info.session_data;
		send_data.append("\r\n.\r\n");
		return send_data;
	}

	bool is_can_start_quit(int status_code)
	{
		//smtp 状态码 250为正常
		//250 2.0.0 Ok: queued as 561416C01FD
		if (status_code == 250)
		{
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}

	pystring_ext send_cmd_quit()
	{
		n_send_setp = SEND_SETP_WAIT_QUIT;
		return "quit\r\n";
	}

	bool is_can_quit(int status_code)
	{
		//smtp 状态码 221为正常
		//221 2.0.0 Bye
		if (status_code == 221)
		{
			n_send_setp = SEND_SETP_QUIT;
			return true;
		}
		n_session_status = STATUS_ERROR;
		return false;
	}
	pystring_ext from_revc_get_next_data(const pystring_ext &rawdata)
	{
		std::vector<pystring_ext> vec_command = rawdata.py_split(" ", 1);
		if (vec_command.size() < 2)
		{
			n_session_status = STATUS_ERROR;
			return "";
		}
		pystring_ext str_status_code = vec_command[0];
		int status_code = (int)str_status_code.to_long64();
		switch (n_send_setp)
		{
		case SEND_SETP_EHLO:
			if (is_can_start_ehlo(status_code) == true)
			{
				return send_cmd_ehlo();
			}
			break;
		case SEND_SETP_MAILFROM:
			if (is_can_start_mail_from(status_code) == true)
			{
				return send_cmd_mail_from();
			}
			break;
		case SEND_SETP_RCPTTO:
			if (is_can_start_rcpt_to(status_code) == true)
			{
				return send_cmd_rcpt_to();
			}
			break;
		case SEND_SETP_DATA:
			if (is_can_start_data(status_code) == true)
			{
				return send_cmd_data_start();
			}
			break;
		case SEND_SETP_DATAEND:
			if (is_can_start_data_end(status_code) == true)
			{
				return send_cmd_data_end();
			}
			break;
		case SEND_SETP_WAIT_DATA_END:
			if (is_can_start_quit(status_code) == true)
			{
				return send_cmd_quit();
			}
			break;
		case SEND_SETP_WAIT_QUIT:
			if (is_can_quit(status_code) == true)
			{
				return "";
			}
		default:
			break;
		}
		n_session_status = STATUS_ERROR;
		return "";
	}
	bool is_command_line(const pystring_ext &rawdata)
	{
		if (rawdata.py_endswith("\n") == true)
		{
			//250-SIZE 33554432
			//250 HELP
			if (rawdata.py_startswith("250-") == true)
			{
				if (rawdata.find("250 ") != std::string::npos)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			if (rawdata.find(" ") != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool is_full_data(const pystring_ext &rawdata)
	{
		if (rawdata.py_endswith("\n") == true)
		{
			if (rawdata.find(" ") != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}
	/*
	bool is_should_close()
	{
	if (n_send_setp == SEND_SETP_QUIT || n_session_status == STATUS_ERROR)
	{
	return true;
	}
	return false;
	}
	*/
};

class smtp_server_protocol_class
{
public:
	enum { SMTP_STATUS_COMMAND, SMTP_STATUS_DATA, SMTP_STATUS_DATAEND, SMTP_STATUS_QUIT };
	volatile int smtp_session_status;
	int session_data_length;
	smtp_info_class smtp_info;
	std::vector<smtp_info_class> vec_smtp_info;
	smtp_server_protocol_class()
		: smtp_session_status(SMTP_STATUS_COMMAND),
		session_data_length(0),
		smtp_info(),
		vec_smtp_info()
	{}

	pystring_ext command_helo(const pystring_ext &info)
	{
		smtp_info.session_command_helo = info;
		return "250 OK\r\n";
	}

	pystring_ext command_ehlo(const pystring_ext &info)
	{
		smtp_info.session_command_ehlo = info;
		return "250-SMTP Protocol\n250 OK\r\n";
	}

	pystring_ext command_mail_form(
		const pystring_ext &str_info,
		const pystring_ext &str_extend_info
	)
	{
		smtp_info.session_command_mail_from =
			str_info.py_between_start_and_end_string(":<", ">");
		if (smtp_info.session_command_mail_from.empty() == true)
		{
			return "501 Syntax error\r\n";
		}
		if (str_extend_info.empty() != true)
		{
			pystring_ext str_datalen =
				str_extend_info.py_lower()
				.py_between_start_and_end_string("size=")
				.py_rstrip();
			if (str_datalen.empty() != true)
			{
				session_data_length = (int)str_datalen.to_long64();
			}
		}
		return "250 2.0 OK\r\n";
	}

	pystring_ext command_rcpt_to(const pystring_ext &str_info)
	{
		pystring_ext str_rcpt_to_addr = str_info.py_between_start_and_end_string(":<",
			">");
		if (str_rcpt_to_addr.empty() == true)
		{
			return "501 Syntax error\r\n";
		}
		smtp_info.session_command_rcpt_to.push_back(str_rcpt_to_addr);
		return "250 2.1 OK\r\n";
	}

	pystring_ext command_data_start()
	{
		smtp_session_status = SMTP_STATUS_DATA;
		return "354 End data with <CR><LF>.<CR><LF>\r\n";
	}

	pystring_ext command_data_end()
	{
		smtp_session_status = SMTP_STATUS_DATAEND;
		vec_smtp_info.push_back(smtp_info);
		return "250 2.3 OK\r\n";
	}

	pystring_ext command_noop()
	{
		return "250 OK\r\n";
	}

	pystring_ext command_rset()
	{
		smtp_session_status = SMTP_STATUS_COMMAND;
		smtp_info.clear();
		return "250 OK\r\n";
	}

	pystring_ext command_quit()
	{
		smtp_session_status = SMTP_STATUS_QUIT;
		return "221 Bye\r\n";
	}

	pystring_ext read_command(const std::vector<pystring_ext> &vec_command)
	{
		if (vec_command.empty() == true)
		{
			return "501 Syntax error\r\n";
		}
		//for example:
		//mail FROM:<Kristie@check-us-out.com> size=204326
		pystring_ext str_cmd = vec_command[0];
		pystring_ext str_argv = "";
		if (vec_command.size() >= 2)
		{
			str_argv = vec_command[1];
			str_argv = str_argv.py_strip();
		}
		pystring_ext str_argv_ext = "";
		if (vec_command.size() == 3)
		{
			str_argv_ext = vec_command[2];
			str_argv_ext = str_argv_ext.py_strip();
		}
		//转换cmd为小写方便比较
		str_cmd = str_cmd.py_lower();
		//去除末尾的\r\n
		str_cmd = str_cmd.py_strip();
		if (str_cmd == "helo")
		{
			return command_helo(str_argv);
		}
		if (str_cmd == "ehlo")
		{
			return command_helo(str_argv);
		}
		if (str_cmd == "mail")
		{
			return command_mail_form(str_argv, str_argv_ext);
		}
		if (str_cmd == "rcpt")
		{
			return command_rcpt_to(str_argv);
		}
		if (str_cmd == "data")
		{
			return command_data_start();
		}
		if (str_cmd == "noop")
		{
			return command_noop();
		}
		if (str_cmd == "rset")
		{
			return command_rset();
		}
		if (str_cmd == "quit")
		{
			return command_quit();
		}
		return "501 Syntax error\r\n";
	}

	pystring_ext read_raw_data(const pystring_ext &data)
	{
		if (smtp_session_status == SMTP_STATUS_DATAEND)
		{
			smtp_session_status = SMTP_STATUS_COMMAND;
		}
		//|| smtp_session_status == SMTP_STATUS_DATAEND
		if (smtp_session_status == SMTP_STATUS_COMMAND)
		{
			std::vector<pystring_ext> vec_command = data.py_split(" ");
			return read_command(vec_command);
		}
		else if (smtp_session_status == SMTP_STATUS_DATA)
		{
			if (data.py_endswith("\r\n.\r\n") == true)
			{
				//data.slice(0,-5)去掉末尾的\r\n.\r\n
				smtp_info.session_data.append(data.py_slice(0, -5));
				if (session_data_length != 0 &&
					smtp_info.session_data.size() != session_data_length
					)
				{
					command_rset();
					return "500 Requested action not taken:data length error\r\n";
				}
				return command_data_end();
			}
			smtp_info.session_data.append(data);
			return "";
		}
		else
		{
			return "";
		}
	}
	bool is_data_end()
	{
		if (SMTP_STATUS_DATAEND == smtp_session_status)
		{
			return true;
		}
		return false;
	}
	bool is_protocol_close()
	{
		if (SMTP_STATUS_QUIT == smtp_session_status)
		{
			return true;
		}
		return false;
	}

	pystring_ext find_last_sender_ip()
	{
		//Received: from mail.begood.com.cn (unknown [111.75.205.179])
		pystring_ext last_sender_ip_range =
			smtp_info.session_data.py_between_start_and_end_string("(", ")");
		pystring_ext last_sender_ip =
			last_sender_ip_range.py_between_start_and_end_string("[", "]");
		//#define INET6_ADDRSTRLEN 46
		//验证IP长度，IPV4应该大于4，IPV6应该小于46
		if (last_sender_ip.size() > 3 && last_sender_ip.size() < 46)
		{
			return last_sender_ip;
		}
		return "";
	}

	void add_last_sender_ip_to_data_header(const pystring_ext &pystr_ip)
	{
		pystring_ext pystr_header_line = "X-REAL-SENDER-IP: ";
		pystr_header_line.append(pystr_ip);
#ifdef _MSC_VER
		pystr_header_line.append("\r\n");
#else
		pystr_header_line.append("\n");
#endif
		pystr_header_line.append(smtp_info.session_data);
		smtp_info.session_data = pystr_header_line;
	}
};
#endif // __SMTP_PROTOCOL_CLASS_HPP__
