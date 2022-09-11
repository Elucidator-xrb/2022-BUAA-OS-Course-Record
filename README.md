# 2022-BUAA-OS-Course
> 20级信息类-6系 大二下操作系统课程设计

作为记录和留念

## structure
增量式开发，相对最完整版在lab6-challenge（尽管有好多烂尾代码）

- 不同lab在对应分支下，仅保留最后通过强测（labx-strong）后的分支；
- main分支下留存了doc/，是一些资料文档；
- job-scheduling分支是试验性平台osome的一次作业；
- transfer-pipe分支是向跳板机传文件用的

## info
最后几天赶制了lab6挑战性任务，exec部分未能良好实现，不知道最后算不算通过（其实说不定可以糊弄过去，不过没意义）

说实话没学懂OS，课设主要还是各种借鉴（最后debug都是各种代码级直接对比）。目前是希望有空能：

- [ ] 把lab6 challeng任务完成并完善shell
- [ ] 试试MOS移植（至少去玩玩qemu），优化整个MOS的代码
- [ ] 尝试写一个MOS解读网站/应用，借机当作所谓前后端开发的练手
- [ ] 去做做/看看MIT和Tsinghua的课设，听说比咱的MOS好的多 

咕...

## reference
试图当一个url中转站

### 代码援助
- [login256](https://github.com/login256/BUAA-OS-2019/tree/lab6) ：公认经典的祖传代码，但lab3开始就已经有些致命bug产生，后期可作借鉴之一，有些实现还是按自己来较舒服
- [rfhits](https://github.com/rfhits/Operating-System-BUAA-2021) ：README写得很好，lab6最终debug可以照着任务列表一一检查函数实现；中转了往届代码，前两个均对照借鉴过
- [refkxh](https://github.com/refkxh/BUAA_OS_2020Spring) ：借鉴对照过
- [C7ABT](https://github.com/C7ABT/BUAA_OS_2020/tree/master) ： 借鉴对照过
- [Coekjan](https://github.com/Coekjan/SOMOS) ： 默默仰望的神之一，完整的lab6带挑战性任务，实现的是shell环境变量
- [zery](https://www.cnblogs.com/zery-blog/p/14970096.html#autoid-1-6-0) ： 博客，在做挑战性任务时启发过思路，不过其exec的想法我未能实现

### 优化体验
[用VSCode写MOS](https://blog.csdn.net/m0_55988640/article/details/124735517)，个人在lab6时用起来这个，方便舒适。

可以前期用课程跳板机体验下CLI，中后期换成这个。（感觉vscode的remote ssh十分的好用，也算是之后跨机器/跨平台开发利器了）
