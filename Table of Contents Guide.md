# Table of Contents Guide

本指南概览项目结构，并对各目录与核心模块做简要介绍，便于快速导航与二次开发。

- 目录
  - 项目结构总览
  - 目录说明
  - 核心模块速览

---

## 项目结构总览

```text
CMakeLists.txt
LICENSE
README.md
img/
	1.png
	2.png
	3.png
include/
	Acceptor.h
	AsyncLogging.h
	Buffer.h
	Callbacks.h
	Channel.h
	ConsistenHash.h
	CurrentThread.h
	EPollPoller.h
	EventLoop.h
	EventLoopThread.h
	EventLoopThreadPool.h
	FileUtil.h
	FixedBuffer.h
	InetAddress.h
	LFU.h
	LogFile.h
	Logger.h
	LogStream.h
	memoryPool.h
	noncopyable.h
	Poller.h
	RICachePolicy.h
	Socket.h
	TcpConnection.h
	TcpServer.h
	Thread.h
	Timer.h
	TimerQueue.h
	Timestamp.h
lib/
	liblog_lib.so
	libmemory_lib.so
	libsrc_lib.so
log/
	AsyncLogging.cc
	CMakeLists.txt
	CurrentThread.cc
	FileUtil.cc
	LogFile.cc
	LogStream.cc
memory/
	CMakeLists.txt
	memoryPool.cc
src/
	Acceptor.cc
	Buffer.cc
	Channel.cc
	CMakeLists.txt
	CurrentThread.cc
	DefaultPoller.cc
	EPollPoller.cc
	EventLoop.cc
	EventLoopThread.cc
	EventLoopThreadPool.cc
	InetAddress.cc
	Logger.cc
	main.cc
	Poller.cc
	Socket.cc
	TcpConnection.cc
	TcpServer.cc
	Thread.cc
	Timer.cc
	TimerQueue.cc
	Timestamp.cc
```

---

## 目录说明

- `include/`：公共头文件，暴露各子模块 API（事件循环、网络、缓存、日志、内存池等）。
- `src/`：核心网络库与服务器的实现文件（Reactor/epoll、TCP 封装、事件/计时器、线程等）。
- `log/`：异步日志子模块的实现与其独立的 `CMakeLists.txt`（`AsyncLogging`、`LogFile`、`LogStream`、`FileUtil` 等）。
- `memory/`：内存池子模块（`memoryPool`）及其 `CMakeLists.txt`。
- `lib/`：预编译的共享库产物（.so，多为 Linux 环境产物，Windows 构建时通常不使用）。
- `img/`：文档配图。
- `CMakeLists.txt`：根构建脚本，子目录分别包含各自的 `CMakeLists.txt`。
- `README.md`：项目说明与使用文档。
- `LICENSE`：开源协议。

---

## 核心模块速览

- 事件与 I/O 框架
  - `EventLoop`、`Channel`、`Poller`/`EPollPoller`：基于 epoll 的 Reactor 事件循环，负责 I/O 事件分发与回调。
  - `Timer`、`TimerQueue`、`Timestamp`：高效的定时器管理与时间戳工具。
- 网络层封装
  - `TcpServer`、`TcpConnection`、`Acceptor`、`Socket`、`InetAddress`：TCP 服务器端模型与连接管理、地址封装。
- 线程与并发
  - `Thread`、`EventLoopThread`、`EventLoopThreadPool`、`CurrentThread`：线程抽象与多 Reactor 线程池支持。
- 缓冲与协议编排
  - `Buffer`、`FixedBuffer`、`Callbacks`：网络缓冲区与回调接口，便于协议编解码与数据聚合。
- 日志系统
  - `Logger`、`LogStream`、`LogFile`、`FileUtil`、`AsyncLogging`：前端日志接口 + 后台异步落盘链路，降低 I/O 抖动对时延影响。
- 内存与缓存策略
  - `memoryPool`：简单高效的内存池实现，减少频繁分配带来的开销。
  - `LFU`、`RICachePolicy`、`ConsistenHash`：提供常用缓存/一致性哈希策略以便进行负载分布或数据分片（文件名按项目现状保留）。
- 通用工具
  - `noncopyable`：防拷贝基类约束。

---

如需进一步了解构建与运行方式，请查看根目录下的 `README.md`。
