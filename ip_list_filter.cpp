#include "ip_list_filter.h"
#include <time.h>
#include <iostream>
#define DEFAULT_MAX_IP_FILTER_LIST_SIZE 100
#define DEFAULT_MAX_IP_DUP_IN_LIST 30
int ip_list_filter::conf_max_ip_list_size = DEFAULT_MAX_IP_FILTER_LIST_SIZE;
int ip_list_filter::conf_max_ip_dup_num = DEFAULT_MAX_IP_DUP_IN_LIST;
ip_list_filter::ip_list_filter() :n_discard(0)
{
}

ip_list_filter::~ip_list_filter()
{
}

void ip_list_filter::count_discard()
{
    n_discard++;
    if(n_discard%100 == 0)
    {
		time_t time_obj;
		time(&time_obj);
		std::cout << time_obj << ", discard number by ip :" << n_discard << std::endl;
    }
}

bool ip_list_filter::is_will_discard(const std::string &str_input)
{
    //return false;
	if (conf_max_ip_list_size <= conf_max_ip_dup_num)
	{
		return false;
	}
	//如果队列里数量小于最大list数量
	/*
	if (m_filter_list.size() < conf_max_ip_list_size)
	{
		//m_filter_list.push_back(str_input);
		return false;
	}
	*/
	//锁住防止多线程冲突
	m_mutex.lock();
	bool b_ret = false;
	int n_dup_num = 0;
	for (
		std::list<std::string>::iterator it = m_filter_list.begin();
		it != m_filter_list.end();
		it++
		)
	{
		if (*it == str_input)
		{
			n_dup_num++;
			if (n_dup_num > conf_max_ip_dup_num)
			{
				b_ret = true;
				break;
			}
		}
	}
	//统计
	if(b_ret == true)
    {
        count_discard();
    }
	//解锁
	m_mutex.unlock();
	return b_ret;
}

void ip_list_filter::push(const std::string &str_input)
{
	//锁住防止多线程冲突
	m_mutex.lock();
	if (m_filter_list.size() < conf_max_ip_list_size)
	{
		m_filter_list.push_back(str_input);
	}
	else
	{
		m_filter_list.pop_front();
		m_filter_list.push_back(str_input);
	}
	//解锁
	m_mutex.unlock();
}
