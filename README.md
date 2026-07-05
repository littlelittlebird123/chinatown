# Chinatown

## 项目简介

本项目是《程序设计》课程大作业，实现了桌游 **Chinatown（唐人街）** 的数字化版本。

游戏支持：

- 四人联机对战
- 单机模式
- 地块抽取
- 玩家交易
- 建设阶段
- 回合结算
- 排行榜结算

## 开发环境

- C++
- Qt 6（Qt Creator）
- CMake / qmake

## 项目结构

```
src/ core/           游戏核心逻辑
     ui/             图形界面
     network/        网络通信模块
     server/         服务器程序
resources/      图片、地图、音乐等资源
```

## 编译运行

### Qt Creator

打开 `ChinatownGame.pro`，配置 Qt Kit 后即可编译运行。

### CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 演示视频

北大网盘：
https://disk.pku.edu.cn/link/AREA513AA4DBF54C779BE854330F871E1D
文件名：98小组演示视频.mp4


## 小组成员

- 杨茗朝
- 刘昊东
- 王国鉴

