//
//  sio_test_sample.cpp
//
//  Created by Melo Yao on 3/24/15.
//  Modified by Haitian Jiang on 2024-03-37.
//

/* g++ main.cpp -std=c++11 -lsioclient */

#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <sio_client.h>
#include <iomanip>

#include <common.h>

#define SOCKETIO_SERVER_PORT "49159"

#define HIGHLIGHT(__O__) std::cout << "\e[1;31m" << __O__ << "\e[0m" << std::endl

using namespace sio;
using namespace std;

static std::mutex _lock;

static std::condition_variable_any _cond;
static bool connect_finish = false;

static socket::ptr current_socket;
static sio::client h;

class connection_listener
{
    sio::client &handler;

public:
    connection_listener(sio::client &h) : handler(h)
    {
    }

    void on_connected()
    {
        _lock.lock();
        _cond.notify_all();
        connect_finish = true;
        _lock.unlock();
    }
    void on_close(client::close_reason const &reason)
    {
        std::cout << "sio closed " << std::endl;
    }

    void on_fail()
    {
        std::cout << "sio failed " << std::endl;
        printf(ANSI_FG_RED "SDB TUI: Failed to connect to sdb server."
                           "Please check if the vsoc server is running and the port is correct." ANSI_NONE "\n");
        exit(1);
    }
};

extern "C" void open_sdb_client()
{
    connection_listener l(h);

    h.set_open_listener(std::bind(&connection_listener::on_connected, &l));
    h.set_close_listener(std::bind(&connection_listener::on_close, &l, std::placeholders::_1));
    h.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

    h.set_reconnect_attempts(2); // attempt to reconnect 2 times
    h.connect("http://127.0.0.1:" SOCKETIO_SERVER_PORT);

    /**
     * [2024-03-28 23:17:18] [info] asio async_connect error: asio.system:111 (Connection refused)
     * [2024-03-28 23:17:18] [info] Error getting remote endpoint: asio.system:107 (Transport endpoint is not connected)
     * [2024-03-28 23:17:18] [error] handle_connect error: Connection refused
     */

    _lock.lock();
    if (!connect_finish)
    {
        _cond.wait(_lock);
        // printf("Waiting for connection ...\n");
    }
    _lock.unlock();
    current_socket = h.socket();
}

static void sdb_client_send_message(string command)
{
    if (!connect_finish)
    {
        return;
    }
    // send command
    current_socket->emit("cmd", command);
    // = "init disas /home/jht/ysyx/six/ysyx2406-jht/am-kernels/kernels/yield-os/yield-os.c";
}

extern "C" void close_sdb_client()
{
    if (connect_finish)
    {
        HIGHLIGHT("Closing sdb client ...");
        h.sync_close();
        h.clear_con_listeners();
    }
}

/**********************************************************/
/*
 * type
 * - 0: bin --> txt
 * - 1: bin --> elf
 * */
extern "C" void init_sdb_vscode_file(char *img_file_path, int type)
{
    // printf("%s\n", img_file_path);
    // /home/jht/ysyx/six/ysyx2406-jht/am-kernels/tests/cpu-tests/build/add-riscv32-nemu.bin

    string file = string(img_file_path); // it's a copy
    // .bin --> .txt, get disas file
    size_t len = file.length();

    if (type == 0)
    {
        file[len - 3] = 't';
        file[len - 2] = 'x';
        file[len - 1] = 't';
    }
    else if (type == 1)
    {
        file[len - 3] = 'e';
        file[len - 2] = 'l';
        file[len - 1] = 'f';
    }

    cout << file << endl;

    if (type == 0)
    {
        sdb_client_send_message("init disas " + file);
    }
    else if (type == 1)
    {
        sdb_client_send_message("init gdb " + file);
    }
}

#define FMT_MY_WORD MUXDEF(CONFIG_ISA64, "%016" PRIx64, "%08" PRIx32)

/**
 * @param type: 0: highlight disassembly file line
 *              1: highlight source file line
 *              2: 0 & 1, concurrent execution
 */
extern "C" void highlight_line(word_t addr, int type)
{
    // word_t --> hex string
    char addr_str[256];
    snprintf(addr_str, 256, FMT_MY_WORD, addr);

    // send cmd
    string addr_string = addr_str;

    switch (type)
    {
    case 0:
        sdb_client_send_message("hl disas " + addr_string);
        break;

    case 1:
        sdb_client_send_message("hl src " + addr_string);
        break;

    case 2:
        sdb_client_send_message("hl all " + addr_string);
        break;

    default:
        break;
    }
}