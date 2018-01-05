#include "proc_limit.h"



proc_limit::proc_limit():m_proc_num(0)
{
	//m_proc_num = 128;
}


proc_limit::~proc_limit()
{
}

//INC(Increment) : ¼ÓÒ»
void proc_limit::INC()
{
	m_proc_num++;
}

//DEC(Decrement): ¼õÒ»
void proc_limit::DEC()
{
	m_proc_num--;
}

int proc_limit::get_number()
{
    return m_proc_num;
}

bool proc_limit::is_need_wait(const int max_proc)
{
	volatile bool b_ret = false;
	m_mutex.lock();
	if (max_proc > m_proc_num)
	{
		INC(); 
	}
	else
	{
		b_ret = true;
	}
	m_mutex.unlock();
	return b_ret;
}
