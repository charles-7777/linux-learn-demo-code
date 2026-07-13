#!/bin/bash


# 启动程序（后台运行）
sudo ./link_monitor &
PID=$!
sleep 2

# 测试链路事件
echo "=== 测试链路事件 ==="
sudo ip link add dummy0 type dummy
sleep 1
sudo ip link set dummy0 up
sleep 1
sudo ip link set dummy0 down
sleep 1
sudo ip link delete dummy0

# 发送信号测试
echo "=== 测试信号 ==="
kill -USR1 $PID
sleep 1
kill -USR2 $PID

# 停止程序
echo "=== 停止程序 ==="
kill $PID
wait $PID 2>/dev/null

# 清理
sudo ip link delete dummy0 2>/dev/null
echo "测试完成"