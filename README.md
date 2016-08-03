#### imSQL-Rewind
imSQL-Rewind的主要功能是把imSQL生成的JSON格式的慢查询日志和审计日志导入到MongoDB中。

# 1.实现方案：
## 1.1.直接打印日志到MongoDB
参考imSQL项目中的audit_to_mongo分支,直接修改audit_log.c文件添加MongoDB的写入代码。使imSQL直接把JSON格式的日志打印到MongoDB中。
但是这样实现有个非常致命的缺陷: 无论是MongoDB的问题还是网络问题,都会使imSQL的所有操作挂起,服务不可用。所以废弃了这种实现。

## 1.2.通过工具把imSQL的日志导入到MongoDB
既然第一种实现方案存在这么大的问题，所以后来改良了实现方案。
imSQL把JSON格式的审计日志和慢查询日志先写到本地磁盘，控制好每个文件的大小和文件个数。
然后用现在的rewind_audit和rewind_slow命令定时读取文件，并且导入到MongoDB中。
测试后发现这种实现方案不会影响imSQL的正常运转。

# 2.编译和运行
    本软件依赖mongo c driver和sqlite，所以编译之前除了要确保系统中有GCC，还要把这两个依赖装上。
## 2.1.直接编译方式
本软件已经写好了Makefile文件，所以可以直接make编译出二进制文件。
`# cd imSQL-Rewind;make`
## 2.2.编译成rpm包的方式
`# git clone xxx/imSQL-Rewind /root/rpmbuild/SOURCES/imSQL-Rewind`
`# cp /root/rpmbuild/SOURCES/imSQL-Rewind/build-ps/imSQL-Rewind.spec /root/rpmbuild/SPECS`
`# rpmbuild -ba /root/rpmbuild/SPECS/imSQL-Rewind.spec`

