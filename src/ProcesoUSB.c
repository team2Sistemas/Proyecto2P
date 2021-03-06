#include <libudev.h>
#include <stdio.h>
#include <mntent.h>
#include <unistd.h>
#include <string.h>
//Includes para daemonizar
#include "apue.h"
#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>


//Funcion para daemonizar
void daemonize(const char *cmd){
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    /*
     * Clear file creation mask.
     */
    umask(0);

    /*
     * Get maximum number of file descriptors.
     */
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        err_quit("%s: can't get file limit", cmd);

    /*
     * Become a session leader to lose controlling TTY.
     */
    if ((pid = fork()) < 0)
        err_quit("%s: can't fork", cmd);
    else if (pid != 0) /* parent */
        exit(0);
    setsid();

    /*
     * Ensure future opens won't allocate controlling TTYs.
     */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        err_quit("%s: can't ignore SIGHUP", cmd);
    if ((pid = fork()) < 0)
        err_quit("%s: can't fork", cmd);
    else if (pid != 0) /* parent */
        exit(0);

    /*
     * Change the current working directory to the root so
     * we won't prevent file systems from being unmounted.
     */
    if (chdir("/") < 0)
        err_quit("%s: can't change directory to /", cmd);

    /*
     * Close all open file descriptors.
     */
    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
    for (i = 0; i < rl.rlim_max; i++)
        close(i);

    /*
     * Attach file descriptors 0, 1, and 2 to /dev/null.
     */
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    /*
     * Initialize the log file.
     */
    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d",
          fd0, fd1, fd2);
        exit(1);
    }
}


//Hay que hacer sudo apt-get install libudev_dev

//Estructura que guardara la informacion de los usb

struct udev_device* obtener_hijo(struct udev* udev, struct udev_device* padre, const char* subsistema);

static void enumerar_disp_alm_masivo(struct udev* udev);

struct udev_device* obtener_hijo(struct udev* udev, struct udev_device* padre, const char* subsistema){
	struct udev_device* hijo = NULL;
	struct udev_enumerate *enumerar = udev_enumerate_new(udev);

	udev_enumerate_add_match_parent(enumerar,padre);
	udev_enumerate_add_match_subsystem(enumerar, subsistema);
	udev_enumerate_scan_devices(enumerar);

	struct udev_list_entry *dispositivos = udev_enumerate_get_list_entry(enumerar);
	struct udev_list_entry *entrada;

	udev_list_entry_foreach(entrada,dispositivos){
		const char *ruta = udev_list_entry_get_name(entrada);
		hijo = udev_device_new_from_syspath(udev, ruta);
		break;
	}

	udev_enumerate_unref(enumerar);
	return hijo;
}

static void enumerar_disp_alm_masivo(struct udev* udev){
	struct udev_enumerate* enumerar = udev_enumerate_new(udev);

	//Buscamos los dispositivos USB del tipo SCSI (MASS STORAGE)
	udev_enumerate_add_match_subsystem(enumerar, "scsi");
	udev_enumerate_add_match_property(enumerar, "DEVTYPE", "scsi_device");
	udev_enumerate_scan_devices(enumerar);

	//Obtenemos los dispositivos con dichas caracteristicas
	struct udev_list_entry *dispositivos = udev_enumerate_get_list_entry(enumerar);
	struct udev_list_entry *entrada;

	//Recorremos la lista obtenida
	udev_list_entry_foreach(entrada, dispositivos) 
	{
		const char* ruta = udev_list_entry_get_name(entrada);
		struct udev_device* scsi = udev_device_new_from_syspath(udev, ruta);

		//Obtenemos la informacion pertinente del dispositivo
		struct udev_device* block = obtener_hijo(udev, scsi, "block");
		struct udev_device* scsi_disk = obtener_hijo(udev, scsi, "scsi_disk");

		struct udev_device* usb = udev_device_get_parent_with_subsystem_devtype(scsi, "usb", "usb_device");

        	struct mntent *m;	
		FILE *f;
		f = setmntent("/etc/mtab", "r");
		char *nombre;
		
		if (block && scsi_disk && usb){
			/*estructura *arg_estructura = (estructura *)malloc(sizeof(estructura *));
			arg_estructura->nodo = udev_device_get_devnode(block);
			arg_estructura->idVendor = udev_device_get_sysattr_value(usb, "idVendor");
			arg_estructura->idProduct = udev_device_get_sysattr_value(usb, "idProduct");*/
			while((m = getmntent(f))){
				nombre = m->mnt_fsname;
				if (strstr(nombre,udev_device_get_devnode(block)) != NULL){
					printf("block = %s, usb=%s:%s, scsi=%s, direccion=%s\n",
					udev_device_get_devnode(block),
					udev_device_get_sysattr_value(usb, "idVendor"),
					udev_device_get_sysattr_value(usb, "idProduct"),
					udev_device_get_sysattr_value(scsi, "vendor"), m->mnt_dir);
				}
			}
			endmntent(f);
		}
		if (block){
			udev_device_unref(block);
		}
		if(scsi_disk){
			udev_device_unref(scsi_disk);
		}
		udev_device_unref(scsi);
	}
	udev_enumerate_unref(enumerar);
}

int main(int argc, char *argv[]){
	//daemonize();
	struct udev *u = udev_new();	
	while(1){
		enumerar_disp_alm_masivo(u);
		sleep(2);
	}
	return 0;

}
