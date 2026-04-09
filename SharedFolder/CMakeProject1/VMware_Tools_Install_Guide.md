# VMware Tools 安装指南

根据您提供的错误信息，VMware Tools 不再随旧版客户机操作系统的 VMware Workstation 一起提供。以下是手动下载和安装的步骤。

## 1. 下载 VMware Tools ISO 镜像

由于自动下载失败，您需要手动下载对应的 ISO 文件。

*   **下载链接**: [https://packages-prod.broadcom.com/tools/frozen/linux/linux.iso](https://packages-prod.broadcom.com/tools/frozen/linux/linux.iso)
    *   *注意：此链接对应 Linux 客户机。如果您使用的是 Windows 客户机（如 Windows 7/XP），请查找对应的 `windows.iso`。*

## 2. 将 ISO 挂载到虚拟机

1.  下载完成后，将 `linux.iso` 文件保存到宿主机（您的物理机）上容易找到的位置。
2.  打开 VMware Workstation。
3.  右键点击您的虚拟机，选择 **设置 (Settings)**。
4.  选择 **CD/DVD (SATA/IDE)** 设备。
5.  在右侧选择 **使用 ISO 映像文件 (Use ISO image file)**。
6.  点击 **浏览 (Browse)**，选择您刚才下载的 `linux.iso` 文件。
7.  确保勾选 **已连接 (Connected)** 和 **启动时连接 (Connect at power on)**。
8.  点击 **确定** 保存设置。

## 3. 在客户机操作系统中安装 (以 Linux 为例)

1.  启动虚拟机并登录。
2.  在虚拟机内部，挂载光驱（通常会自动挂载到 `/media/cdrom` 或 `/mnt/cdrom`）。如果未自动挂载：
    ```bash
    sudo mkdir /mnt/cdrom
    sudo mount /dev/cdrom /mnt/cdrom
    ```
3.  解压安装包：
    ```bash
    cd /tmp
    tar zxpf /mnt/cdrom/VMwareTools-x.x.x-yyyy.tar.gz
    ```
    *(请将文件名替换为实际看到的版本号)*
4.  **运行安装脚本**：
    解压完成后，进入解压出来的目录并运行安装程序：
    ```bash
    cd /tmp/vmware-tools-distrib
    sudo ./vmware-install.pl
    ```
5.  **一路按 Enter 键**
    安装程序会问很多问题（比如安装路径、功能模块等）。
    *   **直接按回车键 (Enter)** 接受所有默认设置即可。
    *   直到看到 "Enjoy, --the VMware team" 的提示，说明安装成功。

### 如果输错了选项怎么办？(Reinstalling)

如果您在安装过程中不小心输错了（例如默认是 `[no]` 您输了 `y`），或者想重新安装：

1.  **直接重新运行安装脚本**
    再次执行安装命令：
    ```bash
    sudo ./vmware-install.pl
    ```
2.  **处理旧版本提示**
    安装程序可能会提示检测到已安装的版本，询问是否要继续安装。
    *   输入 **yes** 并按回车。
    *   它会自动覆盖之前的设置。

## 安装后在哪里？(Verification)

VMware Tools 主要作为**后台服务**运行，通常**不会**在桌面上创建图标。您可以通过以下方式确认它是否在工作：

1.  **验证安装版本**
    在终端输入：
    ```bash
    vmware-toolbox-cmd -v
    ```
    如果能看到版本号（例如 `10.3.25`），说明安装成功。

2.  **共享文件夹 (Shared Folders)**
    如果您在虚拟机设置中启用了共享文件夹，它们默认位于：
    ```bash
    /mnt/hgfs
    ```
    *(如果目录是空的，可能需要先在 VMware 设置里添加共享文件夹，然后重启)*

### 故障排除：共享文件夹 /mnt/hgfs 为空

如果您在虚拟机设置中已经启用了“总是启用 (Always enabled)”，但 `/mnt/hgfs` 还是空的，请尝试以下步骤：

1.  **手动挂载 (临时生效)**
    执行以下命令尝试手动挂载：
    ```bash
    # 确保挂载点存在
    sudo mkdir -p /mnt/hgfs
    
    # 执行挂载命令
    sudo /usr/bin/vmhgfs-fuse .host:/ /mnt/hgfs -o subtype=vmhgfs-fuse,allow_other
    ```
    *如果提示找不到命令，可能是安装不完整，或者路径不同。尝试输入 `vmhgfs-fuse` 并按 Tab 键补全看看。*

2.  **验证**
    再次查看目录：
    ```bash
    ls /mnt/hgfs
    ```
    现在应该能看到您的共享文件夹了。

3.  **设置开机自动挂载 (永久生效)**
    如果手动挂载成功，您可以将其添加到 `/etc/fstab` 文件中，以便开机自动挂载。
    编辑文件：
    ```bash
    sudo nano /etc/fstab
    ```
    在文件末尾添加一行：
    ```
    .host:/ /mnt/hgfs fuse.vmhgfs-fuse allow_other 0 0
    ```
    按 `Ctrl+O` 保存，`Enter` 确认，`Ctrl+X` 退出。

### 仍然找不到？(Advanced Troubleshooting)

如果执行了挂载命令后 `/mnt/hgfs` 依然是空的，请按顺序检查：

1.  **关键检查：虚拟机是否“看”到了共享文件夹？**
    在终端执行：
    ```bash
    vmware-hgfsclient
    ```
    *   **情况 A：没有任何输出**
        说明虚拟机根本不知道有共享文件夹。
        *   **解决**：回到 Windows 的 VMware 界面 -> 虚拟机设置 -> 选项 -> 共享文件夹。
        *   先选择 **“禁用”**，点击确定。
        *   再次打开设置，选择 **“总是启用”**，并确保下面的列表里**有**文件夹。
    
    *   **情况 B：输出了您的文件夹名字**
        说明连接正常，只是挂载点有问题。
        *   尝试更简单的挂载命令：
            ```bash
            sudo mount -t fuse.vmhgfs-fuse .host:/ /mnt/hgfs -o allow_other
            ```
        *   或者检查目录是否存在：
            ```bash
            sudo mkdir -p /mnt/hgfs
            ```

    *   **情况 C：输出为空，或者只显示目录名但没内容**
        如果您运行 `ls /mnt/hgfs` 只看到 `/mnt/hgfs:` 然后下面什么都没有，或者 `vmware-hgfsclient` 有输出但 `ls` 为空：
        *   这说明挂载**没有成功**，或者挂载了一个空列表。
        *   **强制卸载并重试**：
            ```bash
            sudo umount -f /mnt/hgfs
            sudo /usr/bin/vmhgfs-fuse .host:/ /mnt/hgfs -o subtype=vmhgfs-fuse,allow_other
            ```
        *   **检查内核模块**：
            有时候内核更新后模块没加载。尝试重新加载驱动：
            ```bash
            sudo modprobe fuse
            ```
            然后再次尝试挂载。

    *   **情况 D：看到了文件夹名 (SharedFolder/) 但进不去或没内容**
        如果 `ls` 显示了 `SharedFolder/`，说明挂载**已经成功了**！
        *   您需要**进入**这个文件夹才能看到里面的文件：
            ```bash
            cd /mnt/hgfs/SharedFolder
            ls
            ```
        *   不要只看 `/mnt/hgfs`，它只是一个入口。

### 特殊情况：能看到文件夹名，但里面没文件

如果您在 `/mnt/hgfs` 下能看到您的共享文件夹（例如 `Shared`），但点进去是空的：

1.  **权限问题**
    尝试用管理员权限查看：
    ```bash
    sudo ls -R /mnt/hgfs
    ```
    如果 `sudo` 能看到文件，说明是权限问题。您可以在挂载命令中指定您的用户 ID（uid）和组 ID（gid）：
    ```bash
    # 先查看您的 uid 和 gid (通常是 1000)
    id
    
    # 卸载旧的
    sudo umount /mnt/hgfs
    
    # 带权限挂载 (将 1000 替换为您实际的 id)
    sudo /usr/bin/vmhgfs-fuse .host:/ /mnt/hgfs -o subtype=vmhgfs-fuse,allow_other,uid=1000,gid=1000
    ```

2.  **Windows 路径错误**
    回到 Windows 的 VMware 设置 -> 共享文件夹。
    *   选中您的文件夹，点击 **属性 (Properties)**。
    *   检查 **主机路径 (Host Path)** 是否正确指向了包含文件的那个文件夹。
    *   有时候路径指错了（比如指到了一个空文件夹），Linux 里自然也就看不到东西。

3.  **功能测试**
    *   **屏幕自适应**：尝试改变 VMware 窗口的大小，Linux 的分辨率应该会自动调整。
    *   **复制粘贴**：尝试从 Windows 复制一段文字，粘贴到 Linux 的文本编辑器中。

6.  **重启虚拟机**：
    ```bash
    sudo reboot
    ```

## 常见问题解决 (Troubleshooting)

### 错误：无法从终端读取归档内容 / No such file or directory

如果您在执行 `tar` 命令时遇到错误，通常是因为光驱没有挂载，或者挂载路径不是 `/mnt/cdrom`。请按照以下步骤排查：

1.  **确认光驱设备名称**
    在终端运行：
    ```bash
    lsblk
    ```
    查看是否有 `sr0` 或 `cdrom` 设备。

2.  **手动挂载光驱**
    如果光驱未挂载，请执行：
    ```bash
    # 创建挂载点（如果不存在）
    sudo mkdir -p /mnt/cdrom
    
    # 挂载设备（假设设备是 /dev/sr0）
    sudo mount /dev/sr0 /mnt/cdrom
    ```
    *如果提示 "write-protected, mounting read-only"，说明挂载成功。*

3.  **确认文件路径**
    挂载成功后，查看光驱里的具体文件名：
    ```bash
    ls /mnt/cdrom
    ```
    您应该能看到类似 `VMwareTools-10.x.x-xxxx.tar.gz` 的文件。

4.  **重新执行解压命令**
    使用确切的文件名（不要使用通配符 `*`，或者确保路径正确）：
    ```bash
    # 示例（请根据上一步 ls 的结果修改文件名）
    tar zxpf /mnt/cdrom/VMwareTools-10.3.25-18557794.tar.gz -C /tmp
    ```

### 错误：特殊设备 /dev/sr0 不存在 (Special device /dev/sr0 does not exist)

如果系统提示找不到 `/dev/sr0`，说明 Linux 客户机没有识别到光驱硬件。请按以下顺序检查：

1.  **检查 VMware 连接状态 (最常见原因)**
    *   查看 VMware 窗口右下角的状态栏，找到 CD/DVD 图标（通常是一个光盘图标）。
    *   如果图标是灰色的，右键点击它，选择 **连接 (Connect)**。
    *   或者在菜单栏选择：**虚拟机 (VM)** -> **可移动设备 (Removable Devices)** -> **CD/DVD** -> **连接 (Connect)**。

2.  **查找正确的设备名称**
    有些旧版 Linux 系统可能不使用 `sr0`，而是使用 `hdc` 或 `cdrom`。
    运行以下命令列出所有块设备：
    ```bash
    lsblk
    ```
    或者：
    ```bash
    cat /proc/partitions
    ```
    寻找大小与您的 ISO 文件相近（约几百 MB）的设备。如果是 `hdc`，则挂载命令应改为：
    ```bash
    sudo mount /dev/hdc /mnt/cdrom
    ```

3.  **检查虚拟机设置**
    *   如果以上都无效，请关闭虚拟机。
    *   编辑虚拟机设置，确保 **CD/DVD** 设备存在。如果不存在，点击 **添加 (Add)** -> **CD/DVD 驱动器**。
    *   确保勾选 **启动时连接 (Connect at power on)**。

### 警告：客户机操作系统已将 CD-ROM 门锁定 (CD-ROM door locked)

如果您在尝试断开连接或更改 ISO 时看到此提示：
> "客户机操作系统已将 CD-ROM 门锁定... 确实要断开连接并覆盖锁定设置吗?"

这是因为 Linux 系统当前**正在挂载**该光盘，为了防止数据丢失，系统锁定了光驱。

**解决方法：**

1.  **方法 A：在虚拟机内卸载 (推荐)**
    先在终端中输入以下命令来“释放”光驱：
    ```bash
    sudo umount /mnt/cdrom
    ```
    (或者 `sudo umount /dev/sr0`)
    执行完后，您就可以在 VMware 设置中自由断开或更改光驱了。

2.  **方法 B：强制断开**
    如果您只是想快速更改设置，并且当前**没有**正在从光盘复制文件，您可以直接点击提示框中的 **“是 (Yes)”**。这会强制断开连接，通常不会对系统造成损坏，只是相当于强行把光盘拔出来了。

## 参考文档

*   Broadcom 知识库文章: [https://knowledge.broadcom.com/external/article?legacyId=1014294](https://knowledge.broadcom.com/external/article?legacyId=1014294)
