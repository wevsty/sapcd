#include <iostream>
#include <string>
using namespace std;
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "asio_async_tcp_server_template.hpp"
#include "asio_async_tcp_client_template.hpp"
#include "async_smtp_scanner.h"

class SAPCD_CONFIG
{
public:
	po::options_description desc;
	po::variables_map vm;
	string bind_addr;
	string bind_port;
	string realy_addr;
	string realy_port;
	string spamd_addr;
	string spamd_port;
	int n_thread_number;
	int n_io_max_timeouts;
	int n_max_ip_filter_list;
	int n_max_ip_dup_num;
    int n_max_size_filter_list;
	int n_max_size_dup_num;
	int n_max_scan_size;
	//int n_max_wait_time;
	int n_max_porc;
	//int n_disable_filter;

	SAPCD_CONFIG(int argc, char*argv[])
		:desc("Allowed options"),vm(),
		bind_addr(""), bind_port(""),
		realy_addr(""), realy_port(""),
		spamd_addr(""), spamd_port(""),
		n_thread_number(0), n_io_max_timeouts(0),
		n_max_ip_filter_list(0), n_max_ip_dup_num(0),
		n_max_size_filter_list(0), n_max_size_dup_num(0),
		n_max_scan_size(0),
		//n_max_wait_time(0),
		n_max_porc(0)
	{
		desc.add_options()
			("help,h", "produce help message")
			//("help,h", "produce help message")
			("bind_addr", po::value<string>(&bind_addr)->default_value("127.0.0.1"), "Listen IP address(default:127.0.0.1).")
			("bind_port", po::value<string>(&bind_port)->default_value("10025"), "Listen TCP port(default:10025).")
			("realy_addr", po::value<string>(&realy_addr)->default_value("127.0.0.1"), "Listen IP address(default:127.0.0.1).")
			("realy_port", po::value<string>(&realy_port)->default_value("10026"), "Listen TCP port(default:10026).")
			("spamd_addr", po::value<string>(&spamd_addr)->default_value("127.0.0.1"), "Listen IP address(default:127.0.0.1).")
			("spamd_port", po::value<string>(&spamd_port)->default_value("783"), "Listen TCP port(default:783).")
			("threads", po::value<int>(&n_thread_number)->default_value(boost::thread::hardware_concurrency()), "Worker thread number(default:CPU Core number).")
			("timouts", po::value<int>(&n_io_max_timeouts)->default_value(80), "Socket IO timeout second(default:80).")
			("max_ip_filter_list", po::value<int>(&n_max_ip_filter_list)->default_value(100), "ip filter list maximum size(default:100).")
			("max_ip_dup_num", po::value<int>(&n_max_ip_dup_num)->default_value(100), "Maximum duplicate number in ip filter list(default:100).")
            ("max_size_filter_list", po::value<int>(&n_max_size_filter_list)->default_value(100), "ip filter list maximum size(default:100).")
			("max_size_dup_num", po::value<int>(&n_max_size_dup_num)->default_value(100), "Maximum duplicate number in ip filter list(default:100).")
			("max_scan_size", po::value<int>(&n_max_scan_size)->default_value(2048), "Maximum size of mail to scan (in KB).(default:2048KB)")
			//("max_wait_time", po::value<int>(&n_max_wait_time)->default_value(60), "Maximum wait spamd time.(default:60 second)")
			("max_proc", po::value<int>(&n_max_porc)->default_value(16), "Maximum spamd parallel porc.(default:16)")
			;
		//("compression", po::value<int>(&level)->default_value(1), "set compression level");
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
		if (vm.count("help"))
		{
			cout << desc << endl;
			return;
		}
		//change byte to kb
		n_max_scan_size = n_max_scan_size *1024;
		#ifdef _DEBUG
		n_thread_number = 1;
		//n_max_ip_filter_list = 10;
		//n_max_size_filter_list = 10;
		#endif
	}
	bool is_need_exited() const
	{
		if (vm.count("help"))
		{
			return true;
		}
		return false;
	}
};

const SAPCD_CONFIG *g_sapcd_config = NULL;

void set_config(const SAPCD_CONFIG& sapcd_config)
{
	//设置ip过滤参数
	ip_list_filter::conf_max_ip_dup_num = sapcd_config.n_max_ip_dup_num;
	ip_list_filter::conf_max_ip_list_size = sapcd_config.n_max_ip_filter_list;
	//设置大小过滤参数
	data_size_list_filter::conf_max_size_dup_num = sapcd_config.n_max_size_dup_num;
	data_size_list_filter::conf_max_size_list_size = sapcd_config.n_max_size_filter_list;
	//smtp_scanner_server_session::conf_session_ip_filter;
	smtp_scanner_server_session::conf_session_timeout = sapcd_config.n_io_max_timeouts;
	//
	async_spamd_client::conf_spamd_host = sapcd_config.spamd_addr;
	async_spamd_client::conf_spamd_port = sapcd_config.spamd_port;
	async_spamd_client::conf_spamd_endpoint =
		async_spamd_client::reslove_endpoint(sapcd_config.spamd_addr, sapcd_config.spamd_port);
	async_spamd_client::conf_spamd_timeout = sapcd_config.n_io_max_timeouts;
	async_spamd_client::conf_spamd_max_scanner_bytes = sapcd_config.n_max_scan_size;
	async_spamd_client::conf_spamd_max_session = sapcd_config.n_max_porc;
	//
	smtp_client_session::conf_smtp_to_host = sapcd_config.realy_addr;
	smtp_client_session::conf_smtp_to_port = sapcd_config.realy_port;
	smtp_client_session::conf_smtp_to_endpoint =
		smtp_client_session::reslove_endpoint(sapcd_config.realy_addr, sapcd_config.realy_port);
	smtp_client_session::conf_smtp_client_timeout = sapcd_config.n_io_max_timeouts;

}

int main(int argc, char*argv[])
{
	SAPCD_CONFIG sapcd_config(argc,argv);
	if (sapcd_config.is_need_exited() == true)
	{
		return 0;
	}
	g_sapcd_config = &sapcd_config;
    //#ifdef _DEBUG
	//sapcd_config.bind_addr = "0.0.0.0";
	//#endif // _DEBUG
	//加载配置
	set_config(sapcd_config);
	try
	{
		boost::asio::io_service io;
		async_tcp_server<smtp_scanner_server_session> svrt(io, sapcd_config.n_thread_number,
			sapcd_config.bind_addr, sapcd_config.bind_port);
		svrt.run();
		svrt.wait();
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
	return 0;
}
