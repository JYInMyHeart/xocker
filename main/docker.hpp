#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <cstring>
#include <string>
#include <net/if.h>
#include <arpa/inet.h>
#include "network.h"

#define STACK_SIZE (512 * 512)

namespace docker{
	typedef int proc_statu;
	proc_statu proc_err = -1;
	proc_statu proc_exit = 0;
	proc_statu proc_wait = 1;

typedef struct container_config{
	std::string host_name;
	std::string root_dir;
	std::string ip;
	std::string bridge_name;
	std::string bridge_ip;
}container_config;

class container{
private:
	char *veth1;
	char *veth2;
	typedef int process_pid;
	char child_stack[STACK_SIZE];
	container_config config;
	void start_bash(){
		std::string bash = "/bin/bash";
		char *c_bash = new char[bash.length()+1];
		strcpy(c_bash,bash.c_str());
		char* const child_args[] = {c_bash,NULL};
		execv(child_args[0],child_args);
		delete []c_bash;
	}
	void set_hostname(){
		sethostname(this->config.host_name.c_str(),this->config.host_name.length());
	
	}
	void set_rootdir(){
		chdir(this->config.root_dir.c_str());
		chroot(".");
	}
	void set_procsys(){
		mount("none","/proc","proc",0,nullptr);
		mount("none","/sys","sysfs",0,nullptr);
	}
	void set_network(){
		int ifindex = if_nametoindex("eth0");
		struct in_addr ipv4;
		struct in_addr bcast;
		struct in_addr gateway;
		inet_pton(AF_INET,this->config.ip.c_str(),&ipv4);
		inet_pton(AF_INET,"255.255.255.0",&bcast);
		inet_pton(AF_INET,this->config.bridge_ip.c_str(),&gateway);
		lxc_ipv4_addr_add(ifindex,&ipv4,&bcast,16);
		lxc_netdev_up("lo");
		lxc_netdev_up("eth0");
		lxc_ipv4_gateway_add(ifindex,&gateway);
		char mac[18];
		new_hwaddr(mac);
		setup_hw_addr(mac,"eth0");
	}
public:

	container(container_config &config){
		this->config = config;
	}
	void start(){
		char veth1buf[IFNAMSIZ] = "shiyanlou0X";
		char veth2buf[IFNAMSIZ] = "shiyanlou0X";
		veth1 = lxc_mkifname(veth1buf);
		veth2 = lxc_mkifname(veth2buf);
		lxc_veth_create(veth1,veth2);
		setup_private_host_hw_addr(veth1);
		lxc_bridge_attach(config.bridge_name.c_str(),veth1);
		lxc_netdev_up(veth1);
		auto setup = [](void *args) -> int {
			auto _this = reinterpret_cast<container *>(args);
			_this->set_hostname();
			_this->set_rootdir();
			_this->set_procsys();
			_this->set_network();
			_this->start_bash();
			return proc_wait;
		};
		process_pid child_pid = clone(setup,child_stack+STACK_SIZE,CLONE_NEWUTS|CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWNET|SIGCHLD,this);
		lxc_netdev_move_by_name(veth2,child_pid,"eth0");
		waitpid(child_pid,nullptr,0);
	}
	~container(){
		lxc_netdev_delete_by_name(veth1);
		lxc_netdev_delete_by_name(veth2);
	}
	
};

}

