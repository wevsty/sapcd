#if defined(_MSC_VER)
#pragma once
#endif
#ifndef __PROC_LIMIT_CLASS_H__
#define __PROC_LIMIT_CLASS_H__

#include <atomic> // std::atomic
#include <mutex>

class proc_limit
{
public:
	proc_limit();
	~proc_limit();
	//INC(Increment) : º”“ª
	void INC();
	//DEC(Decrement): ºı“ª
	void DEC();
	int get_number();
	bool is_need_wait(const int max_proc);
private:
	std::atomic<int> m_proc_num;
	std::mutex m_mutex;
};

#endif //__PROC_LIMIT_CLASS_H__
