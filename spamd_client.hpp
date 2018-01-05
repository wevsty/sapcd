#pragma once
#include "asio_sync_client.hpp"
#include "spamd_protocol.hpp"

class spamd_client
{
    spamd_client()
    {

    }
    spamd_client(boost::asio::ip::tcp::endpoint spamd_endpoint)
    {
        client_endpoint=spamd_endpoint;
    }
    bool connect(const std::string& host, const std::string& service)
    {
        boost::system::error_code ec
        return client.connect(host,service,timeouts,ec);
    }
    bool connect(boost::asio::ip::tcp::endpoint spamd_endpoint)
    {
        boost::system::error_code ec
        client_endpoint=spamd_endpoint;
        return client.connect(spamd_endpoint,timeouts,ec);
    }
    bool connect()
    {
        boost::system::error_code ec
        return client.connect(client_endpoint,timeouts,ec);
    }
    bool process_spam(const string &str_body,string &ret_response)
    {
        boost::system::error_code ec
        string str_request_body=str_body;
        str_request_body.append('\r\n');
        string str_request = spamd.process(str_request_body);
        if(client.is_open()==false)
        {
            return false;
        }
        if(client.write_some(str_request,timeouts,ec)==false)
        {
            return false;
        }
        string str_buffer;
        string str_response;
        SPAMD_PROTOCOL spamd_protocol;
        while(true)
        {
            if(client.read_some(str_buffer,timeouts,ec)==false)
            {
                return false;
            }
            str_response+=str_buffer;
            if(*str_buffer.rbegin()!='\n')
            {
                continue;
            }
            map<string, string> header_map;
			string str_response_body;
			int n_status = 0;
            if(spamd_protocol.response_load(str_response,n_status,header_map,str_response_body)==true)
            {
                if (n_status != 0)
                {
                    cerr<<"protocol error:"<<str_response<<endl;
                    return false;
                }
                if (spamd.verify_body_length(header_map, str_response_body.length()) != true)
                {
                    continue;
                }
                //pop \r\n
                str_response_body.pop_back();
                str_response_body.pop_back();
                ret_response=str_response_body;
                return true;
            }
            else
            {
                cerr<<"protocol error:"<<str_response<<endl;
                return false;
            }
        }
    }

    int timeouts;
    boost::asio::ip::tcp::endpoint client_endpoint;
    asio_sync_client client;
};

