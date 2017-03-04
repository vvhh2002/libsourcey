///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup base
/// @{


#include "scy/process.h"
#include "scy/filesystem.h"
#include <iostream>


namespace scy {


Process::Process(uv::Loop* loop)
    : uv::Handle(loop, new uv_process_t)
    , _stdin(loop)
    , _stdout(loop)
{
    init();
}


Process::Process(std::initializer_list<std::string> args, uv::Loop* loop)
    : uv::Handle(loop, new uv_process_t)
    , args(args)
    , _stdin(loop)
    , _stdout(loop)
{
    init();
}


Process::~Process()
{
    if (pid())
        kill();
}


void Process::init()
{
    ptr()->data = this;
    options.args = nullptr;
    options.env = nullptr;
    options.cwd = nullptr;
    options.flags = 0;
    options.stdio_count = 0;
    // options.uid = 0;
    // options.gid = 0;
    options.exit_cb = [](uv_process_t* req, int64_t exitStatus, int /*termSignal*/) {
        auto self = reinterpret_cast<Process*>(req->data);
        if (self->onexit)
            self->onexit(exitStatus);
        // We could call close() here to free the uv_process_t content
    };

    _stdin.init();
    _stdout.init();
    _stdout.Read += [this](Stream&, const char* data, const int& len) {
        if (sdout)
            sdout(std::string(data, len));
    };

    options.stdio = _stdio;
    options.stdio[0].flags = uv_stdio_flags(UV_CREATE_PIPE | UV_READABLE_PIPE);
    options.stdio[0].data.stream = _stdin.ptr<uv_stream_t>();
    //options.stdio[0].flags = uv_stdio_flags(UV_IGNORE);
    options.stdio[1].flags = uv_stdio_flags(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    options.stdio[1].data.stream = _stdout.ptr<uv_stream_t>();
    options.stdio_count = 2;
}


void Process::spawn()
{
    // Sanity checks
    //if (options.file == nullptr)
    //    throw std::runtime_error("Cannot spawn process: File path must be set.");
    if (args.size() > 10)
        throw std::runtime_error("Cannot spawn process: Maximum of 10 command line arguments are supported.");

    // Override c style args if STL containers have items.
    _cargs.clear();
    if (!args.empty()) {
        //assert(!!options.args && "setting both args and options.args");
        for (auto& arg : args)
            _cargs.push_back(&arg[0]);
        _cargs.push_back(nullptr);
    }

    if (!cwd.empty()) {
        options.cwd = &cwd[0];
    }

    if (!file.empty()) {
        options.file = &file[0];
        if (_cargs.empty()) {
            _cargs.push_back(&file[0]);
            _cargs.push_back(nullptr);
        }
    }
    else if (!_cargs.empty()) {
        options.file = _cargs[0];
    }

    assert(!_cargs.empty() && "args must not be empty");
    options.args = &_cargs[0];

    // Spawn the process
    int r = uv_spawn(loop(), ptr<uv_process_t>(), &options);
    if (r < 0)
        setAndThrowError("Cannot spawn process", r);

    // Start reading on the stdout pipe
    if (!_stdout.readStart())
        setAndThrowError("Cannot read stdout pipe");
}


Pipe& Process::in()
{
    return _stdin;
}


Pipe& Process::out()
{
    return _stdout;
}


bool Process::kill(int signum)
{
    assert(pid() > 0);
    return uv_kill(pid(), signum) == 0;
}


int Process::pid() const
{
    return ptr<uv_process_t>()->pid;
}


} // namespace scy


/// @\}
