#!/usr/bin/env python3
"""Deploy Fire-Ice server to remote host via SSH (paramiko)."""
import paramiko
import os
import sys
import time
import glob

HOST = "8.141.101.126"
PORT = 22
USER = "root"
PASSWORD = "wll13569035397."
PROJECT = "/root/fire-ice"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCAL_SRC = ROOT


class Deploy:
    def __init__(self):
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    def connect(self):
        print(f"Connecting to {USER}@{HOST}:{PORT} ...")
        self.ssh.connect(HOST, PORT, USER, PASSWORD, timeout=15)
        print("Connected.")

    def run(self, cmd, timeout=120):
        print(f"  > {cmd[:120]}{'...' if len(cmd) > 120 else ''}")
        stdin, stdout, stderr = self.ssh.exec_command(cmd, timeout=timeout)
        out = stdout.read().decode("utf-8", errors="replace")
        err = stderr.read().decode("utf-8", errors="replace")
        ret = stdout.channel.recv_exit_status()
        if ret != 0:
            print(f"  rc={ret}")
        if err.strip():
            for line in err.strip().split("\n")[-5:]:
                print(f"  ERR: {line}")
        return ret, out, err

    def put_file(self, local, remote):
        print(f"  Uploading: {os.path.basename(local)}")
        sftp = self.ssh.open_sftp()
        try:
            sftp.put(local, remote)
        finally:
            sftp.close()

    def put_dir(self, local_dir, remote_dir):
        sftp = self.ssh.open_sftp()
        try:
            # Ensure remote dir exists
            try:
                sftp.mkdir(remote_dir)
            except IOError:
                pass

            for root, dirs, files in os.walk(local_dir):
                rel = os.path.relpath(root, local_dir)
                remote_path = os.path.join(remote_dir, rel).replace("\\", "/")
                for d in dirs:
                    try:
                        sftp.mkdir(os.path.join(remote_path, d).replace("\\", "/"))
                    except IOError:
                        pass
                for f in files:
                    local_file = os.path.join(root, f).replace("\\", "/")
                    remote_file = os.path.join(remote_path, f).replace("\\", "/")
                    try:
                        sftp.put(local_file, remote_file)
                    except Exception as e:
                        print(f"  WARN: {f} - {e}")
        finally:
            sftp.close()

    def close(self):
        self.ssh.close()


def check_env(deploy):
    print("\n=== Server Environment ===")
    for cmd in [
        "cat /etc/os-release | head -4",
        "g++ --version 2>/dev/null | head -1 || echo 'g++ NOT FOUND'",
        "cmake --version 2>/dev/null | head -1 || echo 'cmake NOT FOUND'",
        "dpkg -l 2>/dev/null | grep -i libsfml || rpm -qa 2>/dev/null | grep -i sfml || echo 'SFML packages: need install'",
        "free -h | head -2",
        "df -h / | tail -1",
    ]:
        ret, out, err = deploy.run(cmd)
        print(out.strip())


def install_deps(deploy):
    print("\n=== Installing Dependencies ===")
    ret, out, _ = deploy.run("cat /etc/os-release | head -1")
    os_info = out.lower()

    if "ubuntu" in os_info or "debian" in os_info:
        cmds = [
            "export DEBIAN_FRONTEND=noninteractive && apt-get update -y",
            "export DEBIAN_FRONTEND=noninteractive && apt-get install -y build-essential cmake git libsfml-dev libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev libgl1-mesa-dev libfreetype6-dev libopenal-dev libvorbis-dev libflac-dev",
        ]
    elif "centos" in os_info or "rhel" in os_info or "fedora" in os_info or "rocky" in os_info or "almalinux" in os_info:
        if "centos 7" in os_info:
            cmds = [
                "yum install -y epel-release || true",
                "yum install -y centos-release-scl || true",
                "yum install -y devtoolset-9-gcc devtoolset-9-gcc-c++ cmake3 git",
                "ln -sf /usr/bin/cmake3 /usr/bin/cmake 2>/dev/null || true",
            ]
        else:
            cmds = [
                "dnf install -y epel-release || true",
                "dnf groupinstall -y 'Development Tools' || true",
                "dnf install -y cmake git gcc-c++",
            ]
        cmds.append("rpm -qa | grep sfml || echo 'Will build SFML from source (included in CMake project)'")
    else:
        cmds = [
            "apt-get update -y 2>/dev/null; yum update -y 2>/dev/null; true",
            "(apt-get install -y build-essential cmake git libsfml-dev 2>/dev/null) || (yum install -y cmake gcc-c++ make git 2>/dev/null) || echo 'Manual install may be needed'",
        ]

    for cmd in cmds:
        deploy.run(cmd, timeout=300)
    print("  Dependencies installation attempted.")


def upload_code(deploy):
    print("\n=== Uploading Source Code ===")
    deploy.run(f"mkdir -p {PROJECT}/shared/src {PROJECT}/server/src {PROJECT}/assets/levels {PROJECT}/assets/textures")

    local = LOCAL_SRC.replace("\\", "/")
    script_dir = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")

    # Use server-only CMakeLists
    server_cmake = f"{script_dir}/CMakeLists_server.txt"
    if os.path.exists(server_cmake):
        deploy.put_file(server_cmake, f"{PROJECT}/CMakeLists.txt")
        print("  Uploaded server-only CMakeLists.txt")

    # Files to upload (only what server needs)
    files = [
        (f"{local}/shared/src/Types.hpp", f"{PROJECT}/shared/src/Types.hpp"),
        (f"{local}/shared/src/Protocol.hpp", f"{PROJECT}/shared/src/Protocol.hpp"),
        (f"{local}/shared/src/Map.hpp", f"{PROJECT}/shared/src/Map.hpp"),
        (f"{local}/shared/src/Map.cpp", f"{PROJECT}/shared/src/Map.cpp"),
        (f"{local}/shared/src/Paths.hpp", f"{PROJECT}/shared/src/Paths.hpp"),
        (f"{local}/shared/src/Paths.cpp", f"{PROJECT}/shared/src/Paths.cpp"),
        (f"{local}/shared/src/Physics.hpp", f"{PROJECT}/shared/src/Physics.hpp"),
        (f"{local}/shared/src/Physics.cpp", f"{PROJECT}/shared/src/Physics.cpp"),
        (f"{local}/shared/src/LevelCatalog.hpp", f"{PROJECT}/shared/src/LevelCatalog.hpp"),
        (f"{local}/shared/src/LevelCatalog.cpp", f"{PROJECT}/shared/src/LevelCatalog.cpp"),
        (f"{local}/shared/src/LevelProgress.hpp", f"{PROJECT}/shared/src/LevelProgress.hpp"),
        (f"{local}/shared/src/Pickup.hpp", f"{PROJECT}/shared/src/Pickup.hpp"),
        (f"{local}/shared/src/Pickup.cpp", f"{PROJECT}/shared/src/Pickup.cpp"),
        (f"{local}/shared/src/LevelMechanics.hpp", f"{PROJECT}/shared/src/LevelMechanics.hpp"),
        (f"{local}/shared/src/LevelMechanics.cpp", f"{PROJECT}/shared/src/LevelMechanics.cpp"),
        (f"{local}/shared/src/Room.hpp", f"{PROJECT}/shared/src/Room.hpp"),
        (f"{local}/shared/src/Room.cpp", f"{PROJECT}/shared/src/Room.cpp"),
        (f"{local}/server/src/GameServer.hpp", f"{PROJECT}/server/src/GameServer.hpp"),
        (f"{local}/server/src/GameServer.cpp", f"{PROJECT}/server/src/GameServer.cpp"),
        (f"{local}/server/src/RoomNetwork.hpp", f"{PROJECT}/server/src/RoomNetwork.hpp"),
        (f"{local}/server/src/RoomNetwork.cpp", f"{PROJECT}/server/src/RoomNetwork.cpp"),
        (f"{local}/server/src/ServerRoom.hpp", f"{PROJECT}/server/src/ServerRoom.hpp"),
        (f"{local}/server/src/main.cpp", f"{PROJECT}/server/src/main.cpp"),
    ]

    for local_file, remote_file in files:
        if os.path.exists(local_file):
            deploy.put_file(local_file, remote_file)
        else:
            print(f"  MISSING: {local_file}")

    # Upload assets
    if os.path.exists(f"{local}/assets/levels"):
        deploy.put_dir(f"{local}/assets/levels", f"{PROJECT}/assets/levels")
    if os.path.exists(f"{local}/assets/textures"):
        deploy.put_dir(f"{local}/assets/textures", f"{PROJECT}/assets/textures")

    print("  Upload complete.")


def build(deploy):
    print("\n=== Building Server ===")
    cmds = [
        f"cd {PROJECT} && rm -rf build && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1",
        f"cd {PROJECT}/build && cmake --build . --config Release -j$(nproc) 2>&1",
    ]
    for cmd in cmds:
        ret, out, err = deploy.run(cmd, timeout=600)
        print(out[-800:])
        if ret != 0:
            print("BUILD FAILED!")
            return False
    return True


def setup_firewall(deploy):
    print("\n=== Setting up Firewall ===")
    cmds = [
        "ufw allow 24567/udp 2>/dev/null && echo 'ufw: OK' || true",
        "firewall-cmd --add-port=24567/udp --permanent 2>/dev/null && firewall-cmd --reload 2>/dev/null && echo 'firewalld: OK' || true",
        "iptables -I INPUT -p udp --dport 24567 -j ACCEPT 2>/dev/null && echo 'iptables: OK' || true",
        "iptables -I INPUT -p udp --dport 24567 -j ACCEPT -m state --state NEW 2>/dev/null || true",
        "echo 'Firewall setup attempted. Verify on cloud console: allow UDP 24567'",
    ]
    for cmd in cmds:
        deploy.run(cmd)


def start_server(deploy):
    print("\n=== Starting Server ===")
    deploy.run("pkill -f fireice_server 2>/dev/null; sleep 1; echo 'killed old'", timeout=10)
    time.sleep(1)

    # Use a launch script to daemonize properly
    launch_script = f"""#!/bin/bash
cd {PROJECT}/build
export LD_LIBRARY_PATH={PROJECT}/build/_deps/sfml-build/lib:$LD_LIBRARY_PATH
nohup ./fireice_server > /tmp/fireice_server.log 2>&1 &
echo $!
"""
    deploy.run(f"echo '{launch_script}' > /tmp/launch_server.sh && chmod +x /tmp/launch_server.sh", timeout=10)
    ret, pid_out, _ = deploy.run("bash /tmp/launch_server.sh", timeout=10)
    pid = pid_out.strip()
    print(f"  Started with PID: {pid}")

    time.sleep(3)
    ret, out, _ = deploy.run("ps aux | grep fireice_server | grep -v grep", timeout=10)
    if out.strip():
        print(f"  Server running: {out.strip()}")
    else:
        print("  WARNING: Server process not found! Check log below.")

    _, log, _ = deploy.run("cat /tmp/fireice_server.log", timeout=10)
    print(f"  Log:\n{log.strip()}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python scripts/ssh_deploy.py [deploy|start|status|stop|check]")
        print("  deploy  - Full deploy: env check, install deps, upload, build, firewall, start")
        print("  start   - Start/restart server")
        print("  status  - Show server status and recent logs")
        print("  stop    - Stop server")
        print("  check   - Check server environment")
        sys.exit(1)

    cmd = sys.argv[1]
    d = Deploy()
    d.connect()

    try:
        if cmd in ("deploy", "full"):
            check_env(d)
            install_deps(d)
            upload_code(d)
            if build(d):
                setup_firewall(d)
                start_server(d)
                print("\n=== SERVER READY ===")
                print(f"Clients connect to: {HOST}:24567")
            else:
                print("\nBuild failed. Check errors above.")
        elif cmd == "check":
            check_env(d)
        elif cmd == "start":
            start_server(d)
        elif cmd == "stop":
            print("\n=== Stopping Server ===")
            d.run("pkill -f fireice_server; echo 'stopped'")
        elif cmd == "status":
            ret, out, _ = d.run("ps aux | grep fireice | grep -v grep")
            print("Processes:", out.strip() or "NONE")
            _, log, _ = d.run("tail -30 /tmp/fireice_server.log 2>/dev/null")
            print("Recent logs:", log.strip())
    finally:
        d.close()


if __name__ == "__main__":
    main()
