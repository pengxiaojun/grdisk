## grdisk 磁盘管理工具 ##

grdisk是针对Linux平台的集磁盘管理,ISCSI管理的命令行工具.

##### grdisk主要功能包括: #####

- 列出当前系统所有磁盘
- 磁盘分区管理(创建/删除)
- 磁盘格式化
- 磁盘挂载/卸载
- 读写 /etc/fstab
- 支持ISCSI(发现/登录/登出/initiator读写)

##### Dependency #####
- QT
- DBUS
- udisk2/udisks（grdisk同时支持udisks和udisks2)


##### 配置文件  #####
grdisk默认会读取 grdisk.opt文件来进行初始配置,可配置的选项为(1:开启; 0:不开启):


	listusb=1   #列出usb设备,默认不列出
	listboot=1  #列出系统盘, 默认不列出
	listram=1   #列出ram设备, 默认不列出
	listloop=1  #列出loop设备, 默认不列出
	verbose=1   #verbose模式. 默认关闭
	debug=1     #debug输出. 默认关闭



##### 帮助 #####
	./grdisk help
