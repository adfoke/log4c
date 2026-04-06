# minimal_app

最小接入模板。

## 构建

如果模板还在当前仓库里：

```sh
cmake -S templates/minimal_app -B build-minimal
cmake --build build-minimal
```

如果你把模板拷到别的项目里：

```sh
cmake -S . -B build -DLOG4C_ROOT=/path/to/log4c
cmake --build build
```

## 目录

- `CMakeLists.txt` 通过 `add_subdirectory()` 引入 `log4c`
- `src/main.c` 最小启动代码
- `log4c.conf` 日志配置文件
