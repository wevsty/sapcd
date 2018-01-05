#include "data_size_list_filter.h"
#include <iostream>

#define DEFAULT_MAX_DATASIZE_FILTER_LIST_SIZE 100
#define DEFAULT_MAX_DATASIZE_DUP_IN_LIST 30
int data_size_list_filter::conf_max_size_list_size = DEFAULT_MAX_DATASIZE_FILTER_LIST_SIZE;
int data_size_list_filter::conf_max_size_dup_num = DEFAULT_MAX_DATASIZE_DUP_IN_LIST;

data_size_list_filter::data_size_list_filter() :n_discard(0)
{
}

data_size_list_filter::~data_size_list_filter()
{
}

void data_size_list_filter::count_discard()
{
	n_discard++;
	if (n_discard % 100 == 0)
	{
		time_t time_obj;
		time(&time_obj);
		std::cout << time_obj << ", discard number by data size :" << n_discard << std::endl;
	}
}

bool data_size_list_filter::is_will_discard(const size_t n_data_size)
{
	//转换为KB做单位
	size_t n_data_size_kb = n_data_size / 1024;
	if (conf_max_size_list_size <= conf_max_size_dup_num)
	{
		return false;
	}
	//如果队列里数量小于最大list数量
	/*
	if (m_filter_list.size() < conf_max_size_list_size)
	{
		//m_filter_list.push_back(n_data_size_kb);
		return false;
	}
	*/
	//锁住防止多线程冲突
	m_mutex.lock();
	bool b_ret = false;
	int n_dup_num = 0;
	for (
		std::list<size_t>::iterator it = m_filter_list.begin();
		it != m_filter_list.end();
		it++
		)
	{
		if (*it == n_data_size_kb)
		{
			n_dup_num++;
			if (n_dup_num > conf_max_size_dup_num)
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

void data_size_list_filter::push(const size_t n_data_size)
{
	//转换为KB做单位
	size_t n_data_size_kb = n_data_size / 1024;
	//锁住防止多线程冲突
	m_mutex.lock();
	if (m_filter_list.size() < conf_max_size_list_size)
	{
		m_filter_list.push_back(n_data_size_kb);
	}
	else
	{
		m_filter_list.pop_front();
		m_filter_list.push_back(n_data_size_kb);
	}
	//解锁
	m_mutex.unlock();
}
