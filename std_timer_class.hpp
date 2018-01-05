#ifndef __STD_TIMER_CLASS__
#define __STD_TIMER_CLASS__

//#include <time.h>
#include <ctime>
#define CLOCKS_PER_MS ((CLOCKS_PER_SEC)/1000)
class TIMER_SEC
{
public:
	TIMER_SEC()
	{
	}

	~TIMER_SEC()
	{
	}

	TIMER_SEC operator &(const TIMER_SEC &t1)
	{
		TIMER_SEC t2;
		t2.set_start_time(t1.get_start_time());
		return t2;
	}

	time_t get_start_time() const
	{
		return m_time_start;
	}

	void set_start_time(time_t t_set)
	{
		m_time_start = t_set;
	}

	void start()
	{
		m_time_start = time(0);
	}
	void restart()
	{
		m_time_start = time(0);
	}
	time_t elapsed()
	{
		return time(0) - m_time_start;
	}
private:
	volatile time_t m_time_start;
};

class TIMER_MS
{
public:
	TIMER_MS()
	{
	}

	~TIMER_MS()
	{
	}

	TIMER_MS operator &(const TIMER_MS &t1)
	{
		TIMER_MS t2;
		t2.set_start_time(t1.get_start_time());
		return t2;
	}

	clock_t get_start_time() const
	{
		return m_time_start;
	}

	void set_start_time(clock_t t_set)
	{
		m_time_start = t_set;
	}

	void start()
	{
		m_time_start = clock();
	}
	void restart()
	{
		m_time_start = clock();
	}
	clock_t elapsed()
	{
		//return (clock() - m_time_start);
		return (clock() - m_time_start) / CLOCKS_PER_MS;
	}
private:
	volatile clock_t m_time_start;
};

#endif // !__STD_TIMER_CLASS__
