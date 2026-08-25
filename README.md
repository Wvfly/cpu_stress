# cpu_stress

一个简单的 Linux CPU 负载模拟工具，用于在指定线程数和持续时间内持续占用 CPU 资源，适合测试系统负载、性能压测或验证调度/绑定行为。

## 功能特点

- 支持指定线程数量
- 支持指定运行时长
- 可选择是否绑定 CPU 亲和性
- 支持 SIGINT / SIGTERM 退出信号
- 使用 pthread 和 CPU affinity 机制实现稳定的 CPU 占用

## 编译

```bash
gcc -O2 -Wall -Wextra -pthread cpu_stress.c -o cpu_stress
```

## 使用方法

```bash
./cpu_stress --threads 4 --duration 10
./cpu_stress --threads 8 --duration 60
./cpu_stress --threads 8 --duration 60 --no-affinity
```

## 参数说明

- `--threads N`: 设置工作线程数量
- `--duration SEC`: 运行时长，单位为秒，0 表示无限运行
- `--no-affinity`: 不绑定线程到特定 CPU
- `--help`: 显示帮助信息

## 示例

### 运行 4 个线程，持续 10 秒

```bash
./cpu_stress --threads 4 --duration 10
```

### 运行 8 个线程，持续 60 秒，不绑定 CPU

```bash
./cpu_stress --threads 8 --duration 60 --no-affinity
```

## 说明

该程序会创建多个 CPU worker 线程，并通过忙循环持续消耗 CPU 时间。默认情况下会启用 CPU affinity，将线程分配到不同 CPU 上；如果机器核心数较少，多个线程可能会复用同一颗 CPU。

> 注意：这是一个实际的 CPU 压测工具，使用时请谨慎，避免在生产环境或对系统资源敏感的环境中长时间运行。
