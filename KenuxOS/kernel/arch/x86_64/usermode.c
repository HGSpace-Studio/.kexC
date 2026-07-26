#include <arch/usermode.h>
#include <memory.h>
#include <string.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <interrupt.h>
#include <arch/vga.h>

static user_process_t user_processes[16];
static uint64_t user_process_count = 0;

void usermode_init(void)
{
    memset(user_processes, 0, sizeof(user_processes));
    user_process_count = 0;

    gdt_init();
}

uint64_t usermode_create_process(const char* name, void* entry, uint64_t* pid)
{
    if (user_process_count >= 16) {
        return 0;
    }

    user_process_t* proc = &user_processes[user_process_count];
    proc->entry_point = (uint64_t)entry;
    proc->stack_top = 0x100000;
    proc->heap_start = 0x200000;
    proc->heap_end = 0x300000;
    proc->cr3 = 0;
    proc->page_table = 0;

    if (pid) {
        *pid = user_process_count;
    }

    user_process_count++;
    return user_process_count;
}

void usermode_syscall_entry(void)
{
    __asm__ volatile (
        "pushq %rax\n\t"
        "pushq %rbx\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %rbp\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, %rdi\n\t"
        "call usermode_syscall_handler\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rbp\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rbx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "sysret"
    );
}

void usermode_page_fault_entry(void)
{
    __asm__ volatile (
        "pushq %rax\n\t"
        "pushq %rbx\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %rbp\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %cr2, %rdi\n\t"
        "movq 120(%rsp), %rsi\n\t"
        "call usermode_page_fault_handler\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rbp\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rbx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "iretq"
    );
}

void usermode_syscall_handler(uint64_t* context)
{

    while (1) {}
}

void usermode_page_fault_handler(uint64_t fault_addr, uint64_t error_code)
{

    while (1) {}
}
