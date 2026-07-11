# 构建环境
wsl 中不存在 /lib/modules/6.18.33.2-microsoft-standard-WSL2/build （WSL2 内核没有随附头文件/构建树）
需要自己克隆并编译好的内核源码树
ubuntu 虚拟机中有现成的，可以直接使用
## 下载镜像
https://mirrors.tuna.tsinghua.edu.cn/ubuntu-releases/22.04/ubuntu-22.04.5-live-server-amd64.iso

## 创建并配置虚拟机
- 新建虚拟机：打开 VirtualBox，点击“新建”。输入名称，类型选择“Linux”，版本选择“Ubuntu (64-bit)”。文件夹选择D:\VirtualBox_VMs，这一项用于指定存放虚拟机所有相关文件（包括虚拟硬盘文件 .vdi 和配置文件）的目录。或者（“管理” -> “全局设定”，在“常规”选项卡中，找到 “默认虚拟电脑位置”，将其修改为非系统盘路径D:\VirtualBox_VMs）。勾选跳过自动安装 ，选择下载好的.iso 镜像 ,安装完成后，系统会直接安装在您的虚拟硬盘（.vdi 文件）里后面就不需要了 建议在系统安装完成并重启后，把虚拟光盘“取出来”。
- 硬件：因为是无图形界面的 Server 版，建议分配 2GB 内存即可。 cpu 分配2个处理器 ，启用EFI
- 虚拟硬盘：选择“现在创建虚拟硬盘”，格式推荐 VDI，存储方式选择 动态分配（节省物理磁盘空间），硬盘大小设置为 32GB。
- 点击完成，就创建好了虚拟机
- 点击管理->工具->网络管理器 创建host-only 网卡：右键创建的虚拟机->设置->网络->网卡2->enable ,选择刚刚创建的host-only。网卡仅主机（Host-Only）网络。这能让虚拟机获得独立 IP，方便 Windows 主机通过 SSH 远程连接。
## 启动并执行系统安装
- 右键双击创建的虚拟机，启动虚拟机
- language选择English
- Continue without updating
- keyboard 选English
- 勾选最小化版本
- 网卡配置 跳过
- 代理（Proxy）留空跳过。
- 软件源镜像配置 https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
- 磁盘分区Guided storage configuration 步骤，选择默认的 Use an entire disk（使用整个磁盘），确认自动分区方案并继续
- 账户设置：输入你的全名、服务器主机名（如 ubuntu-dev）、用户名以及密码（请务必牢记此密码，后续 SSH 登录需要）。
- SSH 配置（必选）：在 SSH Setup 步骤，按空格键勾选 Install OpenSSH server（出现 [X] 标记），这是远程连接的通道。
- 附加服务：在 Featured Server Snaps 步骤，全部不选，保持系统绝对干净，直接继续。
- 完成安装：等待进度条走完，系统提示 Install complete! 后，选择 Reboot Now 重启。重启后如果提示移除安装介质，直接按回车即可。
- 进入系统后，ip addr 查看host-only 网卡IP，在windows 中用ssh 登录
- sudo apt update 
- sudo apt install -y man-db vim-tiny net-tools curl wget htop python3 python3-pip dnsutils ufw rsyslog
## 安装编译环境
- sudo apt install -y build-essential linux-headers-$(uname -r)