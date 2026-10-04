"""Windows 10/11 command notepad, Python 3.10+, no third-party dependencies."""
import ctypes as C
from ctypes import wintypes as W
import os
from pathlib import Path
import sys
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, simpledialog


class Sender:
    def __init__(self):
        self.u = C.WinDLL('user32', use_last_error=True)
        for name, args, result in [
            ('GetForegroundWindow', [], W.HWND),
            ('IsWindow', [W.HWND], W.BOOL),
            ('IsWindowVisible', [W.HWND], W.BOOL),
            ('IsIconic', [W.HWND], W.BOOL),
            ('ShowWindow', [W.HWND, C.c_int], W.BOOL),
            ('SetForegroundWindow', [W.HWND], W.BOOL),
            ('GetWindowTextW', [W.HWND, W.LPWSTR, C.c_int], C.c_int),
            ('GetWindowThreadProcessId', [W.HWND, C.POINTER(W.DWORD)], W.DWORD),
        ]:
            fn = getattr(self.u, name)
            fn.argtypes, fn.restype = args, result
        class Keyboard(C.Structure):
            _fields_ = [('vk', W.WORD), ('scan', W.WORD), ('flags', W.DWORD),
                        ('time', W.DWORD), ('extra', C.c_size_t)]
        class Mouse(C.Structure):
            _fields_ = [('x', W.LONG), ('y', W.LONG), ('data', W.DWORD),
                        ('flags', W.DWORD), ('time', W.DWORD), ('extra', C.c_size_t)]
        class Union(C.Union):
            _fields_ = [('keyboard', Keyboard), ('mouse', Mouse)]
        class Input(C.Structure):
            _anonymous_ = ('value',)
            _fields_ = [('kind', W.DWORD), ('value', Union)]
        self.K, self.I = Keyboard, Input
        self.u.SendInput.argtypes = [W.UINT, C.POINTER(Input), C.c_int]
        self.u.SendInput.restype = W.UINT
        self.callback = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
        self.u.EnumWindows.argtypes = [self.callback, W.LPARAM]
        self.u.EnumWindows.restype = W.BOOL

    def pid(self, hwnd):
        value = W.DWORD()
        self.u.GetWindowThreadProcessId(hwnd, C.byref(value))
        return value.value

    def windows(self):
        result = []
        @self.callback
        def visit(hwnd, _):
            title = C.create_unicode_buffer(2048)
            self.u.GetWindowTextW(hwnd, title, len(title))
            pid = self.pid(hwnd)
            if self.u.IsWindowVisible(hwnd) and title.value and pid != os.getpid():
                result.append((hwnd, pid, title.value))
            return True
        self.u.EnumWindows(visit, 0)
        return sorted(result, key=lambda item: item[2].casefold())

    def valid(self, target):
        return target and self.u.IsWindow(target[0]) and self.pid(target[0]) == target[1]

    def activate(self, target):
        if not self.valid(target):
            raise RuntimeError('请绑定有效的目标窗口；原窗口可能已关闭。')
        if self.u.IsIconic(target[0]):
            self.u.ShowWindow(target[0], 9)
        self.u.SetForegroundWindow(target[0])

    def send(self, target, text, enter):
        if not self.valid(target) or self.u.GetForegroundWindow() != target[0]:
            raise RuntimeError('目标窗口未获得焦点，发送已取消。')
        events = []
        raw = text.encode('utf-16-le')
        for offset in range(0, len(raw), 2):
            unit = int.from_bytes(raw[offset:offset + 2], 'little')
            for flags in (4, 6):
                events.append(self.I(kind=1, keyboard=self.K(0, unit, flags, 0, 0)))
        if enter:
            for flags in (0, 2):
                events.append(self.I(kind=1, keyboard=self.K(13, 0, flags, 0, 0)))
        array = (self.I * len(events))(*events)
        if self.u.SendInput(len(events), array, C.sizeof(self.I)) != len(events):
            raise RuntimeError('输入未完整发送，请检查终端内容后再操作。管理员终端需要本工具使用相同权限。')


class App:
    def __init__(self, root):
        self.root, self.sender = root, Sender()
        self.path, self.target, self.busy = None, None, False
        root.geometry('1000x650')
        root.minsize(800, 400)
        bar = ttk.Frame(root, padding=8)
        bar.pack(fill='x')
        for label, fn in [('新建', self.new), ('打开', self.open), ('保存', self.save),
                          ('另存为', lambda: self.save(True)), ('查找', self.find), ('绑定终端', self.choose)]:
            ttk.Button(bar, text=label, command=fn).pack(side='left', padx=2)
        self.enter = tk.BooleanVar(value=False)
        ttk.Checkbutton(bar, text='发送后回车', variable=self.enter).pack(side='left', padx=8)
        self.button = ttk.Button(bar, text='发送当前行 (F8)', command=self.send)
        self.button.pack(side='right')
        self.target_label = ttk.Label(root, text='目标：尚未绑定', padding=8)
        self.target_label.pack(fill='x')
        frame = ttk.Frame(root, padding=8)
        frame.pack(fill='both', expand=True)
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        self.text = tk.Text(frame, undo=True, wrap='none', font=('Consolas', 12), padx=8, pady=8)
        sy = ttk.Scrollbar(frame, command=self.text.yview)
        sx = ttk.Scrollbar(frame, orient='horizontal', command=self.text.xview)
        self.text.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        self.text.grid(row=0, column=0, sticky='nsew')
        sy.grid(row=0, column=1, sticky='ns')
        sx.grid(row=1, column=0, sticky='ew')
        self.text.tag_configure('line', background='#edf4ff')
        self.text.tag_configure('match', background='#ffe08a')
        self.status = ttk.Label(root, text='点击命令所在行，再点击发送。', padding=8)
        self.status.pack(fill='x')
        self.text.bind('<<Modified>>', self.title)
        for event in ('<KeyRelease>', '<ButtonRelease-1>'):
            self.text.bind(event, self.highlight)
        for key, fn in [('n', self.new), ('o', self.open), ('s', self.save), ('f', self.find)]:
            root.bind(f'<Control-{key}>', lambda e, action=fn: (action(), 'break')[1])
        root.bind('<F8>', lambda e: (self.send(), 'break')[1])
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.title()
        self.highlight()

    def title(self, _=None):
        self.root.title(f'{"*" if self.text.edit_modified() else ""}{self.path.name if self.path else "未命名"} — 命令记事本')

    def highlight(self, _=None):
        self.text.tag_remove('line', '1.0', 'end')
        self.text.tag_add('line', 'insert linestart', 'insert lineend +1c')

    def discard(self):
        if not self.text.edit_modified():
            return True
        answer = messagebox.askyesnocancel('保存修改', '是否保存当前文件？')
        return self.save() if answer else answer is False

    def replace(self, content, path):
        self.text.delete('1.0', 'end')
        self.text.insert('1.0', content)
        self.text.mark_set('insert', '1.0')
        self.text.edit_reset()
        self.text.edit_modified(False)
        self.path = path
        self.title()
        self.highlight()

    def new(self):
        if self.discard():
            self.replace('', None)

    def open(self):
        if not self.discard():
            return
        name = filedialog.askopenfilename(filetypes=[('文本文件', '*.txt'), ('所有文件', '*.*')])
        if name:
            try:
                path = Path(name)
                self.replace(path.read_text(encoding='utf-8-sig'), path)
            except (OSError, UnicodeError) as exc:
                messagebox.showerror('打开失败', f'需要 UTF-8 编码的文件。\n{exc}')

    def save(self, save_as=False):
        path = self.path
        if path is None or save_as:
            name = filedialog.asksaveasfilename(defaultextension='.txt')
            if not name:
                return False
            path = Path(name)
        try:
            path.write_text(self.text.get('1.0', 'end-1c'), encoding='utf-8')
            self.path = path
            self.text.edit_modified(False)
            self.title()
            return True
        except OSError as exc:
            messagebox.showerror('保存失败', str(exc))
            return False

    def find(self):
        query = simpledialog.askstring('查找', '输入要查找的内容：')
        if not query:
            return
        pos = self.text.search(query, 'insert', stopindex='end', nocase=True)
        pos = pos or self.text.search(query, '1.0', stopindex='end', nocase=True)
        self.text.tag_remove('match', '1.0', 'end')
        if pos:
            end = f'{pos}+{len(query)}c'
            self.text.tag_add('match', pos, end)
            self.text.mark_set('insert', end)
            self.text.see(pos)
            self.highlight()
        else:
            messagebox.showinfo('查找', '未找到匹配内容。')

    def choose(self):
        dialog = tk.Toplevel(self.root)
        dialog.title('选择终端窗口')
        dialog.geometry('700x380')
        dialog.transient(self.root)
        ttk.Label(dialog, text='命令发送到所选窗口当前激活的标签页 / 输入区域。', padding=10).pack()
        listing = tk.Listbox(dialog, exportselection=False)
        listing.pack(fill='both', expand=True, padx=10)
        windows = []
        def refresh():
            windows[:] = self.sender.windows()
            listing.delete(0, 'end')
            for _, pid, title in windows:
                listing.insert('end', f'{title} [PID {pid}]')
        def bind():
            selection = listing.curselection()
            if selection:
                self.target = windows[selection[0]]
                self.target_label.configure(text=f'目标：{self.target[2]}')
                dialog.destroy()
        bar = ttk.Frame(dialog, padding=10)
        bar.pack(fill='x')
        ttk.Button(bar, text='刷新', command=refresh).pack(side='left')
        ttk.Button(bar, text='绑定', command=bind).pack(side='right')
        listing.bind('<Double-Button-1>', lambda e: bind())
        refresh()

    def send(self):
        if self.busy:
            return
        command = self.text.get('insert linestart', 'insert lineend')
        if not command.strip():
            self.status.configure(text='当前行为空，未发送。')
            return
        if len(command) > 4096 or any(ord(c) < 32 or ord(c) == 127 for c in command):
            messagebox.showerror('无法发送', '命令限 4096 字符，不可包含制表符或控制字符。')
            return
        target, enter = self.target, self.enter.get()
        try:
            self.sender.activate(target)
        except RuntimeError as exc:
            messagebox.showerror('无法发送', str(exc))
            return
        self.busy = True
        self.button.configure(state='disabled')
        line = self.text.index('insert').split('.')[0]
        def finish():
            try:
                self.sender.send(target, command, enter)
                self.status.configure(text=f'已提交第 {line} 行的键盘输入（{"含回车" if enter else "不含回车"}）。')
            except RuntimeError as exc:
                messagebox.showerror('发送失败', str(exc))
            finally:
                self.busy = False
                self.button.configure(state='normal')
        self.root.after(250, finish)

    def close(self):
        if self.discard():
            self.root.destroy()


if __name__ == '__main__':
    if sys.platform != 'win32':
        raise SystemExit('此工具需要 Windows 10/11。')
    root = tk.Tk()
    App(root)
    root.mainloop()
