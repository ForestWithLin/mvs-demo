#!/bin/bash
# bundle_portable.sh - 创建便携式 MVSViewer 包
# 将所有依赖的 .so 收集到执行文件旁，设置 RPATH，可复制到其他 Linux 环境运行
#
# 使用方法:
#   cmake -B build -DPORTABLE_BUILD=ON
#   cmake --build build
#   ./scripts/bundle_portable.sh build
#
# 输出: build/MVSViewer-Portable/ 目录 + build/MVSViewer-Portable.tar.gz

set -e

BUILD_DIR="$1"
if [ -z "$BUILD_DIR" ]; then
    echo "用法: $0 <build_dir>"
    echo "示例: $0 build"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
EXECUTABLE="$BUILD_DIR/MVSViewer"
PORTABLE_DIR="$BUILD_DIR/MVSViewer-Portable"

if [ ! -f "$EXECUTABLE" ]; then
    echo "错误: 执行文件不存在 $EXECUTABLE"
    echo "请先构建项目: cmake --build $BUILD_DIR"
    exit 1
fi

QT_LIB_DIR="/opt/software/Qt6.8.3/lib"
QT_PLUGIN_DIR="/opt/software/Qt6.8.3/plugins"
MVS_LIB_DIR="/opt/MVS/lib/64"

echo "====== 创建便携式包: $PORTABLE_DIR ======"

rm -rf "$PORTABLE_DIR"
mkdir -p "$PORTABLE_DIR/lib/plugins/platforms"

# 1. 复制执行文件
cp "$EXECUTABLE" "$PORTABLE_DIR/MVSViewer"

# 2. 分析执行文件依赖，收集 Qt 库
echo "[1/5] 收集 Qt 库..."
for bin in "$PORTABLE_DIR/MVSViewer" "$QT_PLUGIN_DIR/platforms/libqxcb.so"; do
    [ -f "$bin" ] && ldd "$bin" 2>/dev/null | grep "$QT_LIB_DIR" | awk '{print $3}' | while read -r lib; do
        # 复制所有版本变体
        for f in "$lib"*; do
            [ -f "$f" ] && cp -P "$f" "$PORTABLE_DIR/lib/" 2>/dev/null || true
        done
    done
done

# 3. 复制 Qt 平台插件 (xcb)
echo "[2/5] 复制 Qt 平台插件..."
cp "$QT_PLUGIN_DIR/platforms/libqxcb.so" "$PORTABLE_DIR/lib/plugins/platforms/"

# 4. 复制 MVS SDK 库
echo "[3/5] 复制 MVS SDK 库..."
for f in "$MVS_LIB_DIR"/*.so*; do
    if [ -f "$f" ] || [ -L "$f" ]; then
        cp -P "$f" "$PORTABLE_DIR/lib/"
    fi
done

# 5. 创建 qt.conf - 告诉 Qt 插件位置
echo "[4/5] 创建配置文件..."
cat > "$PORTABLE_DIR/qt.conf" << 'QTCONF'
[Paths]
Plugins = ./lib/plugins
QTCONF

# 6. 创建启动脚本
cat > "$PORTABLE_DIR/run.sh" << 'RUNSCRIPT'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./MVSViewer "$@"
RUNSCRIPT
chmod +x "$PORTABLE_DIR/run.sh"

# 7. 创建压缩包
echo "[5/5] 打包压缩..."
cd "$BUILD_DIR"
rm -f "MVSViewer-Portable.tar.gz"
tar czf "MVSViewer-Portable.tar.gz" "MVSViewer-Portable/"

echo ""
echo "====== 完成 ======"
echo "目录:  $PORTABLE_DIR"
echo "压缩包: $BUILD_DIR/MVSViewer-Portable.tar.gz"
echo ""
echo "在其他机器上运行:"
echo "  tar xzf MVSViewer-Portable.tar.gz"
echo "  cd MVSViewer-Portable"
echo "  ./run.sh"
echo ""
echo "注意: 目标机器需要安装 X11 环境 (libxcb, libxkbcommon 等系统库)"
