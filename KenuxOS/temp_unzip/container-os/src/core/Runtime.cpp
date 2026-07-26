#include "core/Runtime.h"

// Runtime 为抽象基类, 所有方法均为纯虚函数, 由具体运行时 (如 LinuxRuntime /
// WindowsRuntime) 实现具体逻辑。此处仅提供虚析构函数的外联定义, 以确保:
//   1. 析构行为可控, 便于后续扩展公共清理逻辑;
//   2. 避免编译器在多处内联生成虚表, 减小代码体积。
Runtime::~Runtime() = default;
