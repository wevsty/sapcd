#ifndef __SPAMD_PROTOCOL_HPP__
#define __SPAMD_PROTOCOL_HPP__

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include "pystring_extend/pystring_class.h"

#define SPAMD_VERSION_STRING "SPAMC/1.5"

//手动定义协议结尾
#define SPAMD_PROTOCOL_DATA_END_FLAG "\n\r\n"
#define SPAMD_PROTOCOL_DATA_END_FLAG_LEN (3)
class spamd_report
{
public:
	enum {
	SPAMD_HEADER_ERROR,
	SPAMD_REPORT_ERROR,
	SPAMD_DATALEN_ERROR,
	SPAMD_NO_ERROR
	};
private:

	int n_status_code;
	pystring_ext str_spamd_status;
	int n_length;
	bool b_is_spam;
	double ld_score;
	double ld_required;
	pystring_ext str_body_data;

	bool parse_spamd_line(const pystring_ext &line_data)
	{
		std::vector<pystring_ext> vec_line_argv = line_data.py_split(" ");
		if (vec_line_argv.size() < 3)
		{
			return false;
		}
		n_status_code = (int)pystring_ext(vec_line_argv[1]).to_long64();
		str_spamd_status = vec_line_argv[2];
		return true;
	}

	bool parse_content_length(const pystring_ext &line_data)
	{
		std::vector<pystring_ext> vec_line_argv = line_data.py_split(" ");
		if (vec_line_argv.size() < 2)
		{
			return false;
		}
		if (vec_line_argv[1].empty() == true)
		{
			return false;
		}
		n_length = (int)pystring_ext(vec_line_argv[1]).to_long64();
		return true;
	}

	bool parse_spam_status(const pystring_ext &line_data)
	{
		std::vector<pystring_ext> vec_line_argv = line_data.py_split(" ");
		if (vec_line_argv.size() < 6)
		{
			return false;
		}
		if (vec_line_argv[1].empty() == true)
		{
			return false;
		}
		if (vec_line_argv[1] == "True")
		{
			b_is_spam = true;
		}
		ld_score = (double)pystring_ext(vec_line_argv[3]).to_long_double();
		ld_required = (double)pystring_ext(vec_line_argv[5]).to_long_double();
		return true;
	}

	bool parse_header(const pystring_ext &header_data)
	{
		bool b_parse = true;
		std::vector<pystring_ext> header_data_line = header_data.py_split("\r\n");
		pystring_ext pystr_line;
		for (std::vector<pystring_ext>::iterator it = header_data_line.begin();
			it != header_data_line.end();
			it++)
		{
			pystr_line = *it;
			if (pystr_line.py_startswith("SPAMD") == true)
			{
				b_parse = parse_spamd_line(pystr_line);
			}
			else if (pystr_line.py_startswith("Content-length:") == true)
			{
				b_parse = parse_content_length(pystr_line);
			}
			else if (pystr_line.py_startswith("Spam:") == true)
			{
				b_parse = parse_spam_status(pystr_line);
			}
			if (b_parse == false)
			{
				return false;
			}
		}
		return true;
	}
public:

	bool is_full_data(const pystring_ext &raw_data)
	{
		pystring_ext str_full_data_len =
			raw_data.py_between_start_and_end_string("Content-length: ", "\r\n");
		if (str_full_data_len.empty() == true)
		{
			return false;
		}
		size_t n_full_data_len = (int)str_full_data_len.to_long64();
		size_t n_body_start_pos = raw_data.find("\r\n\r\n");
		if (n_body_start_pos == pystring_ext::npos)
		{
			return false;
		}
		else
		{
			//"\r\n\r\n"长度为 4
			n_body_start_pos += 4;
		}
		if (n_full_data_len == raw_data.size() - n_body_start_pos)
		{
			return true;
		}
		return false;
	}

	int parse_report(const pystring_ext &raw_data)
	{
		//不以\r\n\n结尾的肯定不是完整数据
		if (raw_data.py_endswith(SPAMD_PROTOCOL_DATA_END_FLAG) == false)
		{
			return SPAMD_DATALEN_ERROR;
		}
		std::vector<pystring_ext> vec_response_line = raw_data.py_split("\r\n\r\n", 1);
		//如果分割不出header和body则返回错误
		if (vec_response_line.size() < 2)
		{
			return SPAMD_HEADER_ERROR;
		}
		pystring_ext pystr_header = vec_response_line[0];
		pystring_ext pystr_body = vec_response_line[1];
		if (parse_header(pystr_header) == false)
		{
			return SPAMD_HEADER_ERROR;
		}
		if (str_spamd_status != "EX_OK")
		{
			return SPAMD_REPORT_ERROR;
		}
		if (pystr_body.size() != n_length)
		{
			return SPAMD_DATALEN_ERROR;
		}
		//去掉末尾的\r\n\n
		str_body_data = pystr_body.py_slice(0,-SPAMD_PROTOCOL_DATA_END_FLAG_LEN);
		return SPAMD_NO_ERROR;
	}

	bool is_spam()
	{
		return b_is_spam;
	}

	pystring_ext return_body_data()
	{
		return str_body_data;
	}
};

class spamd_request
{
public:
	pystring_ext spamd_check(const pystring_ext &request)
	{
		//末尾要以\r\n结束
		long long new_body_len = request.length() + SPAMD_PROTOCOL_DATA_END_FLAG_LEN;
		pystring_ext package = set_method("CHECK");
		set_header(package, "Content-length",
			pystring_ext::to_pystring_ext(new_body_len));
		set_header_end(package);
		set_request(package, request);
		set_body_end(package);
		return package;
	}
	pystring_ext spamd_process(const pystring_ext &request)
	{
		//末尾要以\r\n结束
		long long new_body_len = request.length() + SPAMD_PROTOCOL_DATA_END_FLAG_LEN;
		pystring_ext package = set_method("PROCESS");
		set_header(package, "Content-length",
			pystring_ext::to_pystring_ext(new_body_len));
		set_header_end(package);
		set_request(package, request);
		set_body_end(package);
		return package;
	}

private:
	inline std::string set_method(const std::string &method_str)
	{
		std::string ret_str = method_str;
		ret_str.append(" " SPAMD_VERSION_STRING "\r\n");
		return ret_str;
	}

	inline void set_header(std::string &package_str, const std::string &header_name,
		const std::string &header_value)
	{
		package_str.append(header_name);
		package_str.append(": ");
		package_str.append(header_value);
		package_str.append("\r\n");
		return;
	}

	inline void set_header_end(std::string &package_str)
	{
		package_str.append("\r\n");
		return;
	}

	inline void set_body_end(std::string &package_str)
	{
		//协议需要自己添加结束标志
		package_str.append(SPAMD_PROTOCOL_DATA_END_FLAG);
		return;
	}

	inline void set_request(std::string &package_str, const std::string &request)
	{
		package_str.append(request);
		return;
	}
};


#endif // !__SPAMD_PROTOCOL_HPP__
