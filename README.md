# Virtual CDROM Driver

一个Windows虚拟DVD驱动程序，可以加载ISO文件并映射为虚拟CD/DVD驱动器。

## 功能特性

- 自动检测 `C:\Program Files (x86)\CDROM\vCD.iso` 文件
- 每10秒检查一次ISO文件（仅当没有媒体插入时）
- 支持手动插入/弹出媒体
- 弹出后不再自动检测，直到设备重启
- 支持标准的SCSI命令集
- 只读设备，防止意外修改ISO内容

## 系统要求

- Windows 10/11 (x64)
- Visual Studio 2022
- Windows Driver Kit (WDK)
- 管理员权限

## 项目结构

```
├── VirtualCdrom.h          # 驱动程序头文件
├── VirtualCdrom.c          # 驱动程序主代码
├── VirtualCdrom.inf        # 驱动安装信息文件
├── VirtualCdrom.vcxproj    # 驱动项目文件
├── VcdControl.c            # 用户模式控制工具
├── VcdControl.vcxproj      # 控制工具项目文件
├── VirtualCdrom.sln        # 解决方案文件
├── build.bat               # 构建脚本
├── install.bat             # 安装脚本
├── uninstall.bat           # 卸载脚本
└── README.md               # 本文件
```

## 构建步骤

1. 安装 Visual Studio 2022（包含C++开发工具）
2. 安装 Windows Driver Kit (WDK)
3. 以管理员身份运行 `build.bat`：

```batch
build.bat
```

构建输出将位于 `build\x64\` 目录：
- `VirtualCdrom.sys` - 驱动程序文件
- `VcdControl.exe` - 控制工具

## 安装

以管理员身份运行安装脚本：

```batch
install.bat
```

安装脚本会：
1. 创建 `C:\Program Files (x86)\CDROM\` 目录
2. 复制驱动程序到系统目录
3. 安装并启动驱动服务

## 使用方法

### 准备ISO文件

将ISO文件复制到：
```
C:\Program Files (x86)\CDROM\vCD.iso
```

驱动会自动检测并加载该文件。

### 控制命令

```batch
# 查看驱动状态
VcdControl.exe status

# 手动插入媒体
VcdControl.exe insert

# 手动弹出媒体
VcdControl.exe eject

# 停止驱动
VcdControl.exe stop

# 启动驱动
VcdControl.exe start
```

### 自动检测逻辑

1. **初始状态**：设备启动时，每10秒检查ISO文件
2. **检测到ISO**：自动插入媒体，停止检测
3. **手动弹出**：进入弹出状态，停止检测直到设备重启
4. **设备重启**：重置为初始状态，重新开始检测

## 卸载

以管理员身份运行卸载脚本：

```batch
uninstall.bat
```

## 技术细节

### 驱动架构

- **类型**：内核模式驱动程序 (KMDF)
- **设备类型**：CD-ROM 设备
- **接口**：SCSI 直通接口

### 支持的SCSI命令

- `INQUIRY` - 设备查询
- `READ_CAPACITY` - 读取容量
- `READ_TOC` - 读取目录
- `READ(10/12/16)` - 读取扇区
- `START_STOP_UNIT` - 启动/停止/弹出
- `MODE_SENSE` - 模式检测
- `TEST_UNIT_READY` - 设备就绪检测

### 状态机

```
NO_MEDIA ──检测到ISO──> MEDIA_INSERTED
    ^                        │
    │                        │ 弹出
    │                        ▼
    └──────────── MEDIA_EJECTED
```

## 故障排除

### 驱动无法启动

1. 检查是否以管理员身份运行
2. 检查Windows安全设置是否阻止未签名驱动
3. 查看系统事件日志获取详细错误信息

### ISO文件无法加载

1. 确认文件路径：`C:\Program Files (x86)\CDROM\vCD.iso`
2. 确认文件格式为标准ISO 9660
3. 检查文件权限

### 设备未显示

1. 运行 `VcdControl.exe status` 检查驱动状态
2. 重新启动驱动服务
3. 检查设备管理器中的虚拟CDROM设备

## 安全说明

⚠️ **警告**：这是一个内核模式驱动程序，具有系统最高权限。

- 仅在测试/开发环境中使用
- 确保ISO文件来源可信
- 卸载前确保没有程序正在访问虚拟驱动器

## 许可证

MIT License

## 作者

Virtual CDROM Project
