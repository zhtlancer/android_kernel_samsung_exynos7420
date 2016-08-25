#!/res/busybox sh

export PATH=/res/asset:$PATH

if [[ "$(cat /data/.arter97/invisible_cpuset)" == "1" ]]; then
	mkdir /dev/cpuset/a_invisible
	chown system:system /dev/cpuset/a_invisible
	chown system:system /dev/cpuset/a_invisible/tasks
	chmod 0755 /dev/cpuset/a_invisible
	chmod 0644 /dev/cpuset/a_invisible/tasks
	echo "0-7" > /dev/cpuset/a_invisible/cpus
	echo "0" > /dev/cpuset/a_invisible/mems
fi

mkdir /dev/cpuset/a_foreground
chown system:system /dev/cpuset/a_foreground
chown system:system /dev/cpuset/a_foreground/tasks
chmod 0755 /dev/cpuset/a_foreground
chmod 0644 /dev/cpuset/a_foreground/tasks
echo "0-7" > /dev/cpuset/a_foreground/cpus
echo "0" > /dev/cpuset/a_foreground/mems

mkdir /dev/cpuset/a_background
chown system:system /dev/cpuset/a_background
chown system:system /dev/cpuset/a_background/tasks
chmod 0755 /dev/cpuset/a_background
chmod 0644 /dev/cpuset/a_background/tasks
echo "0-7" > /dev/cpuset/a_background/cpus
echo "0" > /dev/cpuset/a_background/mems

mkdir /dev/cpuset/a_system-background
chown system:system /dev/cpuset/a_system-background
chown system:system /dev/cpuset/a_system-background/tasks
chmod 0755 /dev/cpuset/a_system-background
chmod 0644 /dev/cpuset/a_system-background/tasks
echo "0-7" > /dev/cpuset/a_system-background/cpus
echo "0" > /dev/cpuset/a_system-background/mems
