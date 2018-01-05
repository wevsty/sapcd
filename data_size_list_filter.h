#if defined(_MSC_VER)
#pragma once
#endif
#ifndef __DATA_SIZE_FILTER_CLASS_H__
#define __DATA_SIZE_FILTER_CLASS_H__
#include <cstddef>
#include <string>
#include <list>
#include <mutex>

class data_size_list_filter
{
public:
	static int conf_max_size_list_size;
	//list中最大允许的重复数量
	static int conf_max_size_dup_num;
	data_size_list_filter();
	~data_size_list_filter();

	bool is_will_discard(const size_t n_data_size);
	void push(const size_t n_data_size);
	void count_discard();
private:
	std::mutex m_mutex;
	std::list<size_t> m_filter_list;
	volatile int n_discard;
};

#endif

