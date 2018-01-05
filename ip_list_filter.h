#if defined(_MSC_VER)
#pragma once
#endif
#ifndef __IP_LIST_FILTER_CLASS_H__
#define __IP_LIST_FILTER_CLASS_H__

#include <string>
#include <list>
#include <mutex>

class ip_list_filter
{
public:
	static int conf_max_ip_list_size;
	//list中最大允许的重复数量
	static int conf_max_ip_dup_num;
	ip_list_filter();
	~ip_list_filter();

	bool is_will_discard(const std::string &str_input);
	void push(const std::string &str_input);
	void count_discard();
private:
	std::mutex m_mutex;
	std::list<std::string> m_filter_list;
	volatile int n_discard;
};

#endif

