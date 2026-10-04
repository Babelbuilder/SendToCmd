// SendToCmd: Windows Forms, .NET Framework 4.8, C# 5.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("SendToCmd")]
[assembly: AssemblyProduct("SendToCmd")]

internal static class Native
{
    [DllImport("user32.dll")] internal static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] internal static extern IntPtr GetAncestor(IntPtr handle, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] internal static extern int GetWindowText(IntPtr handle, StringBuilder text, int count);
    [DllImport("user32.dll")] internal static extern int GetWindowTextLength(IntPtr handle);
    [DllImport("user32.dll")] internal static extern bool IsWindow(IntPtr handle);
    [DllImport("user32.dll")] internal static extern bool IsWindowVisible(IntPtr handle);
    [DllImport("user32.dll")] internal static extern bool IsIconic(IntPtr handle);
    [DllImport("user32.dll")] internal static extern bool ShowWindow(IntPtr handle, int command);
    [DllImport("user32.dll")] internal static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] internal static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] internal static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);
    [DllImport("user32.dll", SetLastError = true)] internal static extern uint SendInput(uint count, [In] Input[] inputs, int size);
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)] internal static extern uint ExtractIconEx(string file, int index, out IntPtr large, out IntPtr small, uint count);
    [DllImport("user32.dll")] internal static extern bool DestroyIcon(IntPtr icon);

    [StructLayout(LayoutKind.Sequential)]
    internal struct KeyboardInput
    {
        internal ushort VirtualKey;
        internal ushort ScanCode;
        internal uint Flags;
        internal uint Time;
        internal IntPtr ExtraInfo;
    }

    // INPUT's union begins at byte 8 on 64-bit Windows, byte 4 on 32-bit Windows.
    [StructLayout(LayoutKind.Explicit, Size = 40)]
    internal struct Input
    {
        [FieldOffset(0)] internal uint Type;
        [FieldOffset(8)] internal KeyboardInput Keyboard;
    }

    internal static Input Key(ushort virtualKey, ushort scanCode, uint flags)
    {
        return new Input { Type = 1, Keyboard = new KeyboardInput { VirtualKey = virtualKey, ScanCode = scanCode, Flags = flags } };
    }
}

internal sealed class TargetWindow
{
    internal IntPtr Handle;
    internal uint ProcessId;
    internal string Title;

    public override string ToString()
    {
        return Title + "  [PID " + ProcessId + "]";
    }

    internal bool IsValid()
    {
        uint current;
        return Native.IsWindow(Handle) && Native.GetWindowThreadProcessId(Handle, out current) != 0 && current == ProcessId;
    }
}

internal sealed class LineHighlightEditor : RichTextBox
{
    internal int ActiveLine;

    protected override void WndProc(ref Message message)
    {
        base.WndProc(ref message);
        if (message.Msg != 0x000F || !IsHandleCreated) return; // WM_PAINT
        int start = GetFirstCharIndexFromLine(ActiveLine);
        if (start < 0) return;
        int y = GetPositionFromCharIndex(start).Y;
        if (y + Font.Height < 0 || y >= ClientSize.Height) return;
        using (Graphics graphics = Graphics.FromHwnd(Handle))
        using (Brush shade = new SolidBrush(Color.FromArgb(52, 182, 193, 241)))
            graphics.FillRectangle(shade, 0, y, ClientSize.Width, Font.Height + 2);
    }
}

internal sealed class TargetCaption : Control
{
    internal TargetCaption()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
    }

    protected override void OnTextChanged(EventArgs args)
    {
        base.OnTextChanged(args);
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs args)
    {
        base.OnPaint(args);
        Rectangle textArea = new Rectangle(Padding.Left, Padding.Top,
            Math.Max(0, Width - Padding.Horizontal), Math.Max(0, Height - Padding.Vertical));
        TextRenderer.DrawText(args.Graphics, Text, Font, textArea, ForeColor,
            TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis |
            TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
    }
}

internal sealed class RoundedSendButton : Button
{
    private bool hovering;
    private bool pressed;
    private bool stopIcon;

    internal bool StopIcon
    {
        get { return stopIcon; }
        set { stopIcon = value; Invalidate(); }
    }

    internal RoundedSendButton()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer, true);
        FlatStyle = FlatStyle.Flat;
        TabStop = true;
    }

    protected override void OnMouseEnter(EventArgs args) { hovering = true; Invalidate(); base.OnMouseEnter(args); }
    protected override void OnMouseLeave(EventArgs args) { hovering = false; pressed = false; Invalidate(); base.OnMouseLeave(args); }
    protected override void OnMouseDown(MouseEventArgs args) { if (args.Button == MouseButtons.Left) pressed = true; Invalidate(); base.OnMouseDown(args); }
    protected override void OnMouseUp(MouseEventArgs args) { pressed = false; Invalidate(); base.OnMouseUp(args); }
    protected override void OnEnabledChanged(EventArgs args) { base.OnEnabledChanged(args); Invalidate(); }

    protected override void OnPaint(PaintEventArgs args)
    {
        args.Graphics.Clear(Parent == null ? Color.White : Parent.BackColor);
        args.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        Rectangle bounds = new Rectangle(1, 1, Math.Max(1, Width - 3), Math.Max(1, Height - 3));
        Color fill = !Enabled ? Color.FromArgb(235, 241, 238) : stopIcon
            ? (pressed ? Color.FromArgb(245, 210, 207) : hovering ? Color.FromArgb(250, 225, 222) : Color.FromArgb(253, 235, 234))
            : pressed ? Color.FromArgb(187, 228, 198)
            : hovering ? Color.FromArgb(208, 239, 216) : Color.FromArgb(222, 246, 228);
        using (GraphicsPath path = RoundedRectangle(bounds, 9))
        using (Brush brush = new SolidBrush(fill))
        using (Pen border = new Pen(Enabled ? (stopIcon ? Color.FromArgb(222, 169, 169) : Color.FromArgb(148, 205, 162)) : Color.FromArgb(211, 223, 215)))
        {
            args.Graphics.FillPath(brush, path);
            args.Graphics.DrawPath(border, path);
        }
        float centerX = bounds.Left + bounds.Width / 2f;
        float centerY = bounds.Top + bounds.Height / 2f;
        float scale = Math.Min(bounds.Width, bounds.Height) / 32f;
        using (Brush symbol = new SolidBrush(!Enabled ? Color.FromArgb(139, 156, 145)
            : stopIcon ? Color.FromArgb(150, 68, 68) : Color.FromArgb(38, 104, 63)))
        {
            if (stopIcon)
            {
                int side = (int)Math.Round(13 * scale);
                Rectangle stop = new Rectangle((int)Math.Round(centerX - side / 2f),
                    (int)Math.Round(centerY - side / 2f), side, side);
                using (GraphicsPath square = RoundedRectangle(stop, Math.Max(2, side / 6)))
                    args.Graphics.FillPath(symbol, square);
            }
            else
            {
                PointF[] plane = {
                    new PointF(centerX - 11 * scale, centerY - 2 * scale),
                    new PointF(centerX + 11 * scale, centerY - 9 * scale),
                    new PointF(centerX + 4 * scale, centerY + 10 * scale),
                    new PointF(centerX - 1 * scale, centerY + 3 * scale),
                    new PointF(centerX - 6 * scale, centerY + 6 * scale),
                    new PointF(centerX - 5 * scale, centerY + 1 * scale)
                };
                args.Graphics.FillPolygon(symbol, plane);
                using (Pen crease = new Pen(fill, Math.Max(1f, scale)))
                    args.Graphics.DrawLine(crease, centerX - 5 * scale, centerY + 1 * scale,
                        centerX + 8 * scale, centerY - 7 * scale);
            }
        }
        if (Focused && ShowFocusCues)
            ControlPaint.DrawFocusRectangle(args.Graphics, Rectangle.Inflate(bounds, -5, -5));
    }

    private static GraphicsPath RoundedRectangle(Rectangle bounds, int radius)
    {
        int diameter = radius * 2;
        GraphicsPath path = new GraphicsPath();
        path.AddArc(bounds.Left, bounds.Top, diameter, diameter, 180, 90);
        path.AddArc(bounds.Right - diameter, bounds.Top, diameter, diameter, 270, 90);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(bounds.Left, bounds.Bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        return path;
    }
}

internal sealed class SendToCmd : Form
{
    private readonly LineHighlightEditor editor = new LineHighlightEditor();
    private readonly Panel lineGutter = new Panel();
    private readonly TargetCaption targetLabel = new TargetCaption();
    private readonly ToolTip targetTip = new ToolTip();
    private readonly Label status = new Label();
    private readonly RoundedSendButton sendButton = new RoundedSendButton();
    private readonly CheckBox autoMode = new CheckBox();
    private readonly ToolTip sendTip = new ToolTip();
    private readonly NumericUpDown intervalSeconds = new NumericUpDown();
    private readonly Timer autoTimer = new Timer();
    private readonly ContextMenuStrip rangeMenu = new ContextMenuStrip();
    private readonly Button targetPicker = new Button();
    private readonly ToolStripMenuItem settingsMenu = new ToolStripMenuItem();
    private readonly ToolStripMenuItem languageMenu = new ToolStripMenuItem();
    private readonly ToolStripMenuItem englishMenu = new ToolStripMenuItem("English");
    private readonly ToolStripMenuItem chineseMenu = new ToolStripMenuItem("中文");
    private readonly Label pickerLabel = new Label();
    private readonly Label intervalLabel = new Label();
    private readonly Label secondLabel = new Label();
    private readonly ToolTip pickerTip = new ToolTip();
    private readonly ToolTip gutterTip = new ToolTip();
    private bool chinese = LoadLanguage();
    private string currentPath;
    private bool dirty;
    private bool loading;
    private bool busy;
    private bool pickingTarget;
    private TargetWindow target;
    private int rangeStart = -1;
    private int rangeEnd = -1;
    private int contextLine = -1;
    private int autoNextLine;
    private int autoEndLine;
    private string[] autoCommands;
    private TargetWindow autoTarget;
    private bool autoRunning;

    internal SendToCmd()
    {
        Text = "SendToCmd";
        IntPtr largeIcon, smallIcon;
        Native.ExtractIconEx(Application.ExecutablePath, 0, out largeIcon, out smallIcon, 1);
        try
        {
            IntPtr titleIcon = largeIcon != IntPtr.Zero ? largeIcon : smallIcon;
            if (titleIcon != IntPtr.Zero) Icon = (Icon)Icon.FromHandle(titleIcon).Clone();
        }
        finally
        {
            if (largeIcon != IntPtr.Zero) Native.DestroyIcon(largeIcon);
            if (smallIcon != IntPtr.Zero) Native.DestroyIcon(smallIcon);
        }
        Width = 1050;
        Height = 700;
        MinimumSize = new Size(780, 420);
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Microsoft YaHei UI", 9);
        BackColor = Color.White;

        MenuStrip menu = new MenuStrip();
        ToolStripMenuItem file = new ToolStripMenuItem();
        file.DropDownItems.Add(Item("New", Keys.Control | Keys.N, delegate { NewFile(); }));
        file.DropDownItems.Add(Item("Open...", Keys.Control | Keys.O, delegate { OpenFile(); }));
        file.DropDownItems.Add(Item("Save", Keys.Control | Keys.S, delegate { SaveFile(false); }));
        file.DropDownItems.Add(Item("Save As...", Keys.Control | Keys.Shift | Keys.S, delegate { SaveFile(true); }));
        file.DropDownItems.Add(new ToolStripSeparator());
        file.DropDownItems.Add(Item("Exit", Keys.None, delegate { Close(); }));
        ToolStripMenuItem edit = new ToolStripMenuItem();
        edit.DropDownItems.Add(Item("Undo", Keys.Control | Keys.Z, delegate { editor.Undo(); }));
        edit.DropDownItems.Add(Item("Cut", Keys.Control | Keys.X, delegate { editor.Cut(); }));
        edit.DropDownItems.Add(Item("Copy", Keys.Control | Keys.C, delegate { editor.Copy(); }));
        edit.DropDownItems.Add(Item("Paste", Keys.Control | Keys.V, delegate { editor.Paste(DataFormats.GetFormat(DataFormats.UnicodeText)); }));
        edit.DropDownItems.Add(Item("Select All", Keys.Control | Keys.A, delegate { editor.SelectAll(); }));
        edit.DropDownItems.Add(Item("Find...", Keys.Control | Keys.F, delegate { FindText(); }));
        menu.Items.Add(file);
        menu.Items.Add(edit);
        englishMenu.Click += delegate { SetLanguage(false); };
        chineseMenu.Click += delegate { SetLanguage(true); };
        languageMenu.DropDownItems.Add(englishMenu);
        languageMenu.DropDownItems.Add(chineseMenu);
        settingsMenu.DropDownItems.Add(languageMenu);
        menu.Items.Add(settingsMenu);
        MainMenuStrip = menu;
        menu.AutoSize = false;
        menu.Dock = DockStyle.Fill;
        menu.Margin = Padding.Empty;
        menu.Padding = new Padding(8, 7, 0, 0);
        menu.BackColor = Color.FromArgb(248, 250, 253);

        TableLayoutPanel topBar = new TableLayoutPanel();
        topBar.Dock = DockStyle.Fill;
        topBar.Margin = Padding.Empty;
        topBar.Padding = Padding.Empty;
        topBar.BackColor = Color.FromArgb(248, 250, 253);
        topBar.ColumnCount = 3;
        topBar.RowCount = 1;
        topBar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260));
        topBar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        topBar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 480));
        topBar.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        topBar.Controls.Add(menu, 0, 0);
        TableLayoutPanel targetArea = new TableLayoutPanel();
        targetArea.Dock = DockStyle.Fill;
        targetArea.Margin = Padding.Empty;
        targetArea.ColumnCount = 2;
        targetArea.RowCount = 1;
        targetArea.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        targetArea.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 155));
        targetArea.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        targetLabel.Dock = DockStyle.Fill;
        targetLabel.Margin = Padding.Empty;
        targetLabel.Padding = new Padding(0, 0, 8, 0);
        targetLabel.ForeColor = Color.FromArgb(86, 101, 119);
        targetLabel.Text = "目标：未绑定";
        targetTip.SetToolTip(targetLabel, targetLabel.Text);
        targetArea.Controls.Add(targetLabel, 0, 0);
        FlowLayoutPanel pickerArea = new FlowLayoutPanel();
        pickerArea.Dock = DockStyle.Fill;
        pickerArea.Margin = Padding.Empty;
        pickerArea.Padding = new Padding(0, 5, 8, 0);
        pickerArea.FlowDirection = FlowDirection.RightToLeft;
        pickerArea.WrapContents = false;
        pickerLabel.Text = "绑定终端";
        pickerLabel.AutoSize = true;
        pickerLabel.Margin = new Padding(3, 5, 4, 0);
        pickerLabel.ForeColor = Color.FromArgb(78, 95, 117);
        targetPicker.Size = new Size(36, 29);
        targetPicker.Margin = new Padding(2, 1, 2, 0);
        targetPicker.AccessibleName = "拖动十字准星绑定终端窗口";
        targetPicker.Cursor = Cursors.Cross;
        targetPicker.BackColor = Color.White;
        targetPicker.FlatStyle = FlatStyle.Flat;
        targetPicker.FlatAppearance.BorderColor = Color.FromArgb(188, 203, 220);
        targetPicker.Paint += DrawCrosshair;
        targetPicker.MouseDown += StartTargetPick;
        targetPicker.MouseMove += MoveTargetPick;
        targetPicker.MouseUp += FinishTargetPick;
        targetPicker.MouseCaptureChanged += CancelLostTargetPick;
        pickerArea.Controls.Add(targetPicker);
        pickerArea.Controls.Add(pickerLabel);
        targetArea.Controls.Add(pickerArea, 1, 0);
        topBar.Controls.Add(targetArea, 2, 0);
        pickerTip.SetToolTip(targetPicker, "按住左键拖到终端窗口，松开后绑定");

        TableLayoutPanel footer = new TableLayoutPanel();
        footer.Dock = DockStyle.Fill;
        footer.Margin = Padding.Empty;
        footer.BackColor = Color.FromArgb(248, 250, 253);
        footer.ColumnCount = 4;
        footer.RowCount = 1;
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 170));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 160));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 68));
        footer.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        footer.Paint += delegate(object sender, PaintEventArgs args)
        {
            using (Pen border = new Pen(Color.FromArgb(225, 231, 239)))
                args.Graphics.DrawLine(border, 0, 0, footer.Width, 0);
        };
        status.Dock = DockStyle.Fill;
        status.Margin = Padding.Empty;
        status.Padding = new Padding(12, 0, 4, 0);
        status.TextAlign = ContentAlignment.MiddleLeft;
        status.AutoEllipsis = true;
        status.Text = "第 1 行 · 发送后自动回车";
        status.ForeColor = Color.FromArgb(103, 118, 136);
        footer.Controls.Add(status, 0, 0);
        FlowLayoutPanel intervalArea = new FlowLayoutPanel();
        intervalArea.Dock = DockStyle.Fill;
        intervalArea.Margin = Padding.Empty;
        intervalArea.Padding = new Padding(0, 10, 0, 0);
        intervalArea.WrapContents = false;
        intervalLabel.Text = "间隔";
        intervalLabel.AutoSize = true;
        intervalLabel.Margin = new Padding(0, 3, 5, 0);
        intervalLabel.ForeColor = Color.FromArgb(86, 101, 119);
        intervalSeconds.DecimalPlaces = 1;
        intervalSeconds.Increment = 0.1M;
        intervalSeconds.Minimum = 0.1M;
        intervalSeconds.Maximum = 60M;
        intervalSeconds.Value = 1M;
        intervalSeconds.Width = 60;
        intervalSeconds.Margin = Padding.Empty;
        intervalSeconds.TextAlign = HorizontalAlignment.Center;
        secondLabel.Text = "秒";
        secondLabel.AutoSize = true;
        secondLabel.Margin = new Padding(4, 3, 0, 0);
        secondLabel.ForeColor = Color.FromArgb(86, 101, 119);
        intervalArea.Controls.Add(intervalLabel);
        intervalArea.Controls.Add(intervalSeconds);
        intervalArea.Controls.Add(secondLabel);
        footer.Controls.Add(intervalArea, 1, 0);
        autoMode.Text = "Auto send";
        autoMode.Dock = DockStyle.Fill;
        autoMode.Margin = new Padding(0, 0, 4, 0);
        autoMode.TextAlign = ContentAlignment.MiddleLeft;
        autoMode.ForeColor = Color.FromArgb(86, 101, 119);
        autoMode.CheckedChanged += delegate { UpdateSendAction(); };
        footer.Controls.Add(autoMode, 2, 0);
        sendButton.Text = String.Empty;
        sendButton.Dock = DockStyle.Fill;
        sendButton.Margin = new Padding(0, 7, 12, 7);
        sendButton.Click += delegate { SendRequested(); };
        footer.Controls.Add(sendButton, 3, 0);

        editor.Dock = DockStyle.Fill;
        editor.Font = new Font("Consolas", 11);
        editor.BorderStyle = BorderStyle.None;
        editor.BackColor = Color.White;
        editor.ForeColor = Color.FromArgb(36, 47, 60);
        editor.WordWrap = false;
        editor.AcceptsTab = false;
        editor.DetectUrls = false;
        editor.HideSelection = false;
        editor.ScrollBars = RichTextBoxScrollBars.Vertical;
        editor.TextChanged += delegate
        {
            if (autoRunning) { StopAuto(false); status.Text = Tr("Text changed; auto send stopped.", "文本已修改，自动发送已停止"); }
            rangeStart = -1;
            rangeEnd = -1;
            if (!loading) { dirty = true; UpdateTitle(); }
            lineGutter.Invalidate();
            editor.Invalidate();
        };
        editor.SelectionChanged += delegate
        {
            editor.ActiveLine = editor.GetLineFromCharIndex(editor.SelectionStart);
            status.Text = LineStatus(editor.ActiveLine);
            lineGutter.Invalidate();
            editor.Invalidate();
        };
        editor.VScroll += delegate { lineGutter.Invalidate(); editor.Invalidate(); };
        editor.MouseWheel += delegate { lineGutter.Invalidate(); editor.Invalidate(); };
        editor.Resize += delegate { lineGutter.Invalidate(); editor.Invalidate(); };
        lineGutter.Dock = DockStyle.Fill;
        lineGutter.Margin = Padding.Empty;
        lineGutter.BackColor = Color.FromArgb(247, 249, 252);
        lineGutter.Paint += DrawLineNumbers;
        lineGutter.MouseDown += SelectLineFromGutter;
        rangeMenu.Items.Add("设为起点（绿点）", null, delegate { SetRangePoint(true); });
        rangeMenu.Items.Add("设为终点（红点）", null, delegate { SetRangePoint(false); });
        rangeMenu.Items.Add(new ToolStripSeparator());
        rangeMenu.Items.Add("清除起点和终点", null, delegate { ClearRange(); });
        rangeMenu.Opening += delegate
        {
            rangeMenu.Items[0].Enabled = !autoRunning;
            rangeMenu.Items[1].Enabled = !autoRunning;
            rangeMenu.Items[3].Enabled = !autoRunning && (rangeStart >= 0 || rangeEnd >= 0);
        };
        lineGutter.ContextMenuStrip = rangeMenu;
        gutterTip.SetToolTip(lineGutter, "左键选行，右键设置自动发送起点和终点");
        autoTimer.Tick += delegate { SendAutoLine(); };
        TableLayoutPanel editorHost = new TableLayoutPanel();
        editorHost.Dock = DockStyle.Fill;
        editorHost.Margin = Padding.Empty;
        editorHost.Padding = Padding.Empty;
        editorHost.ColumnCount = 2;
        editorHost.RowCount = 1;
        editorHost.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 52));
        editorHost.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        editorHost.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        editor.Margin = Padding.Empty;
        editorHost.Controls.Add(lineGutter, 0, 0);
        editorHost.Controls.Add(editor, 1, 0);
        TableLayoutPanel layout = new TableLayoutPanel();
        layout.Dock = DockStyle.Fill;
        layout.Margin = Padding.Empty;
        layout.Padding = Padding.Empty;
        layout.ColumnCount = 1;
        layout.RowCount = 3;
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        layout.Controls.Add(topBar, 0, 0);
        layout.Controls.Add(editorHost, 0, 1);
        layout.Controls.Add(footer, 0, 2);
        Controls.Add(layout);
        FormClosing += OnClosing;
        Shown += delegate { editor.Focus(); };
        ApplyLanguage();
    }

    private static string LanguageFile()
    {
        string appData = Environment.GetEnvironmentVariable("APPDATA");
        if (String.IsNullOrEmpty(appData)) appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        return Path.Combine(appData, "SendToCmd", "language.txt");
    }

    private static bool LoadLanguage()
    {
        try { return File.ReadAllText(LanguageFile()).Trim().Equals("zh", StringComparison.OrdinalIgnoreCase); }
        catch (IOException) { return false; }
        catch (UnauthorizedAccessException) { return false; }
    }

    private bool SaveLanguage()
    {
        try
        {
            string path = LanguageFile();
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, chinese ? "zh" : "en", new UTF8Encoding(false));
            return true;
        }
        catch (IOException) { return false; }
        catch (UnauthorizedAccessException) { return false; }
    }

    private string Tr(string english, string chineseText) { return chinese ? chineseText : english; }
    private string LineStatus(int line) { return chinese ? "第 " + (line + 1) + " 行" : "Line " + (line + 1); }
    private string SentStatus(int line) { return chinese ? "已发送第" + (line + 1) + "行" : "Sent line " + (line + 1); }

    private void SetLanguage(bool useChinese)
    {
        if (chinese == useChinese) return;
        if (autoRunning) StopAuto(true);
        chinese = useChinese;
        ApplyLanguage();
        status.Text = SaveLanguage() ? Tr("Language: English", "语言：中文")
            : Tr("Language changed, but the preference could not be saved.", "语言已切换，但无法保存设置。");
    }

    private void ApplyLanguage()
    {
        ToolStripMenuItem file = (ToolStripMenuItem)MainMenuStrip.Items[0];
        ToolStripMenuItem edit = (ToolStripMenuItem)MainMenuStrip.Items[1];
        file.Text = Tr("&File", "文件(&F)");
        file.DropDownItems[0].Text = Tr("New", "新建");
        file.DropDownItems[1].Text = Tr("Open...", "打开...");
        file.DropDownItems[2].Text = Tr("Save", "保存");
        file.DropDownItems[3].Text = Tr("Save As...", "另存为...");
        file.DropDownItems[5].Text = Tr("Exit", "退出");
        edit.Text = Tr("&Edit", "编辑(&E)");
        edit.DropDownItems[0].Text = Tr("Undo", "撤销");
        edit.DropDownItems[1].Text = Tr("Cut", "剪切");
        edit.DropDownItems[2].Text = Tr("Copy", "复制");
        edit.DropDownItems[3].Text = Tr("Paste", "粘贴");
        edit.DropDownItems[4].Text = Tr("Select All", "全选");
        edit.DropDownItems[5].Text = Tr("Find...", "查找...");
        settingsMenu.Text = Tr("&Settings", "设置(&S)");
        languageMenu.Text = Tr("Language", "语言");
        englishMenu.Checked = !chinese;
        chineseMenu.Checked = chinese;
        targetLabel.Text = Tr("Target: ", "目标：") + (target == null ? Tr("Not bound", "未绑定") : target.Title);
        targetTip.SetToolTip(targetLabel, target == null ? targetLabel.Text : target.Title);
        pickerLabel.Text = Tr("Bind target", "绑定终端");
        targetPicker.AccessibleName = Tr("Drag to bind a terminal window", "拖动十字准星绑定终端窗口");
        pickerTip.SetToolTip(targetPicker, Tr("Hold the left button, drag to the terminal, then release to bind.", "按住左键拖到终端窗口，松开后绑定"));
        intervalLabel.Text = Tr("Interval", "间隔");
        secondLabel.Text = Tr("s", "秒");
        autoMode.Text = Tr("Auto send", "自动发送");
        UpdateSendAction();
        rangeMenu.Items[0].Text = Tr("Set Start (green)", "设为起点（绿点）");
        rangeMenu.Items[1].Text = Tr("Set End (red)", "设为终点（红点）");
        rangeMenu.Items[3].Text = Tr("Clear Start and End", "清除起点和终点");
        gutterTip.SetToolTip(lineGutter, Tr("Left-click to select a line; right-click to set the auto-send range.", "左键选行，右键设置自动发送起点和终点"));
        status.Text = Tr("Line 1 · Enter after sending", "第 1 行 · 发送后自动回车");
        UpdateTitle();
    }

    private void UpdateSendAction()
    {
        sendButton.StopIcon = autoRunning;
        string action = autoRunning ? Tr("Stop auto send (F8)", "停止自动发送 (F8)")
            : autoMode.Checked ? Tr("Auto send range (F8)", "自动发送范围 (F8)")
            : Tr("Send current line (F8)", "发送当前行 (F8)");
        sendButton.AccessibleName = action;
        sendTip.SetToolTip(sendButton, action);
    }

    private static ToolStripMenuItem Item(string text, Keys shortcut, EventHandler action)
    {
        ToolStripMenuItem item = new ToolStripMenuItem(text);
        item.ShortcutKeys = shortcut;
        item.Click += action;
        return item;
    }

    private void DrawLineNumbers(object sender, PaintEventArgs args)
    {
        if (!editor.IsHandleCreated) return;
        int first = editor.GetLineFromCharIndex(editor.GetCharIndexFromPosition(new Point(0, 0)));
        int last = editor.GetLineFromCharIndex(editor.GetCharIndexFromPosition(new Point(0, Math.Max(0, editor.ClientSize.Height - 1))));
        int active = editor.GetLineFromCharIndex(editor.SelectionStart);
        int visibleLast = Math.Min(last, Math.Max(0, editor.Lines.Length - 1));
        for (int line = first; line <= visibleLast; line++)
        {
            int start = editor.GetFirstCharIndexFromLine(line);
            if (start < 0) continue;
            int y = editor.GetPositionFromCharIndex(start).Y;
            if (y + editor.Font.Height < 0 || y >= lineGutter.Height) continue;
            Rectangle row = new Rectangle(0, y, lineGutter.Width - 1, editor.Font.Height + 2);
            if (line == active)
            {
                using (Brush shade = new SolidBrush(Color.FromArgb(228, 235, 251)))
                    args.Graphics.FillRectangle(shade, row);
            }
            TextRenderer.DrawText(args.Graphics, (line + 1).ToString(), editor.Font,
                new Rectangle(0, y, lineGutter.Width - 10, editor.Font.Height + 2),
                line == active ? Color.FromArgb(68, 88, 151) : Color.FromArgb(145, 154, 167),
                TextFormatFlags.Right | TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter);
            if (line == rangeStart || line == rangeEnd)
            {
                args.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
                Rectangle dot = new Rectangle(4, y + (editor.Font.Height - 8) / 2, 9, 9);
                using (Brush green = new SolidBrush(Color.FromArgb(58, 164, 104)))
                using (Brush red = new SolidBrush(Color.FromArgb(211, 89, 91)))
                {
                    if (line == rangeStart) args.Graphics.FillEllipse(green, dot);
                    if (line == rangeEnd)
                    {
                        if (line == rangeStart) args.Graphics.FillPie(red, dot, 270, 180);
                        else args.Graphics.FillEllipse(red, dot);
                    }
                }
            }
        }
        using (Pen border = new Pen(Color.FromArgb(225, 230, 238)))
            args.Graphics.DrawLine(border, lineGutter.Width - 1, 0, lineGutter.Width - 1, lineGutter.Height);
    }

    private void SelectLineFromGutter(object sender, MouseEventArgs args)
    {
        if (autoRunning || (args.Button != MouseButtons.Left && args.Button != MouseButtons.Right)) return;
        int charIndex = editor.GetCharIndexFromPosition(new Point(0, args.Y));
        int line = editor.GetLineFromCharIndex(charIndex);
        int start = editor.GetFirstCharIndexFromLine(line);
        if (start < 0) return;
        if (args.Button == MouseButtons.Right) contextLine = line;
        editor.SelectionStart = start;
        editor.SelectionLength = 0;
        if (args.Button == MouseButtons.Left) editor.Focus();
        lineGutter.Invalidate();
    }

    private void SetRangePoint(bool start)
    {
        if (autoRunning) return;
        int line = contextLine >= 0 ? contextLine : editor.GetLineFromCharIndex(editor.SelectionStart);
        if (start) rangeStart = line; else rangeEnd = line;
        lineGutter.Invalidate();
        status.Text = start ? Tr("Start: line " + (line + 1), "起点：第" + (line + 1) + "行")
            : Tr("End: line " + (line + 1), "终点：第" + (line + 1) + "行");
    }

    private void ClearRange()
    {
        if (autoRunning) return;
        rangeStart = -1;
        rangeEnd = -1;
        lineGutter.Invalidate();
        status.Text = Tr("Auto-send range cleared", "已清除自动发送范围");
    }

    private void AdvanceToNextLine(int sentLine)
    {
        if (sentLine + 1 >= editor.Lines.Length) return;
        int nextStart = editor.GetFirstCharIndexFromLine(sentLine + 1);
        if (nextStart < 0) return;
        editor.SelectionStart = nextStart;
        editor.SelectionLength = 0;
        editor.ScrollToCaret();
        lineGutter.Invalidate();
    }

    protected override bool ProcessCmdKey(ref Message message, Keys keyData)
    {
        if (keyData == Keys.F8)
        {
            SendRequested();
            return true;
        }
        return base.ProcessCmdKey(ref message, keyData);
    }

    private void UpdateTitle()
    {
        Text = "SendToCmd — " + (dirty ? "*" : "") + (currentPath == null ? Tr("Untitled", "未命名") : Path.GetFileName(currentPath));
    }

    private bool ConfirmDiscard()
    {
        if (!dirty) return true;
        DialogResult choice = MessageBox.Show(this, Tr("Save changes to the current file?", "是否保存当前文件？"),
            Tr("Save Changes", "保存修改"), MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
        if (choice == DialogResult.Cancel) return false;
        return choice == DialogResult.No || SaveFile(false);
    }

    private void ReplaceText(string text, string path)
    {
        loading = true;
        editor.Text = text;
        editor.SelectionStart = 0;
        editor.ClearUndo();
        loading = false;
        currentPath = path;
        dirty = false;
        UpdateTitle();
    }

    private void NewFile()
    {
        if (autoRunning) StopAuto(true);
        if (ConfirmDiscard()) ReplaceText("", null);
    }

    private void OpenFile()
    {
        if (autoRunning) StopAuto(true);
        if (!ConfirmDiscard()) return;
        using (OpenFileDialog dialog = new OpenFileDialog())
        {
            dialog.Filter = Tr("Text files (*.txt)|*.txt|All files (*.*)|*.*", "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*");
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            try
            {
                // UTF-8 with or without BOM; reject malformed bytes instead of silently changing commands.
                string text;
                using (StreamReader reader = new StreamReader(dialog.FileName, new UTF8Encoding(false, true), true))
                    text = reader.ReadToEnd();
                ReplaceText(text, dialog.FileName);
            }
            catch (Exception error)
            {
                MessageBox.Show(this, error.Message, Tr("Open Failed", "打开失败"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    private bool SaveFile(bool saveAs)
    {
        string path = currentPath;
        if (path == null || saveAs)
        {
            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = Tr("Text files (*.txt)|*.txt|All files (*.*)|*.*", "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*");
                dialog.DefaultExt = "txt";
                if (path != null) dialog.FileName = Path.GetFileName(path);
                if (dialog.ShowDialog(this) != DialogResult.OK) return false;
                path = dialog.FileName;
            }
        }
        try
        {
            File.WriteAllText(path, editor.Text, new UTF8Encoding(false));
            currentPath = path;
            dirty = false;
            UpdateTitle();
            status.Text = Tr("Saved: ", "已保存：") + path;
            return true;
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message, Tr("Save Failed", "保存失败"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
    }

    private void FindText()
    {
        if (autoRunning) StopAuto(true);
        using (Form dialog = new Form())
        {
            dialog.Text = Tr("Find", "查找");
            dialog.StartPosition = FormStartPosition.CenterParent;
            dialog.FormBorderStyle = FormBorderStyle.FixedDialog;
            dialog.MinimizeBox = false;
            dialog.MaximizeBox = false;
            dialog.ClientSize = new Size(380, 90);
            TextBox query = new TextBox();
            query.SetBounds(12, 14, 355, 24);
            Button find = new Button();
            find.Text = Tr("Find Next", "查找下一个");
            find.SetBounds(255, 50, 110, 27);
            find.Click += delegate
            {
                if (query.Text.Length == 0) return;
                int found = editor.Find(query.Text, editor.SelectionStart + editor.SelectionLength, RichTextBoxFinds.None);
                if (found < 0) found = editor.Find(query.Text, 0, RichTextBoxFinds.None);
                if (found >= 0) { editor.Select(found, query.Text.Length); editor.ScrollToCaret(); }
                else MessageBox.Show(dialog, Tr("No match found.", "未找到匹配内容。"), Tr("Find", "查找"));
            };
            dialog.Controls.Add(query);
            dialog.Controls.Add(find);
            dialog.AcceptButton = find;
            dialog.ShowDialog(this);
        }
    }

    private static void DrawCrosshair(object sender, PaintEventArgs args)
    {
        Button button = (Button)sender;
        int x = button.ClientSize.Width / 2;
        int y = button.ClientSize.Height / 2;
        using (Pen pen = new Pen(Color.FromArgb(30, 70, 120), 2))
        {
            args.Graphics.DrawEllipse(pen, x - 6, y - 6, 12, 12);
            args.Graphics.DrawLine(pen, x - 11, y, x - 3, y);
            args.Graphics.DrawLine(pen, x + 3, y, x + 11, y);
            args.Graphics.DrawLine(pen, x, y - 10, x, y - 3);
            args.Graphics.DrawLine(pen, x, y + 3, x, y + 10);
        }
    }

    private void StartTargetPick(object sender, MouseEventArgs args)
    {
        if (args.Button != MouseButtons.Left) return;
        pickingTarget = true;
        targetPicker.Capture = true;
        status.Text = Tr("Hold the left button, drag the crosshair to a target window, then release.", "按住左键，将十字准星拖到目标窗口后松开。");
    }

    private void MoveTargetPick(object sender, MouseEventArgs args)
    {
        if (!pickingTarget) return;
        TargetWindow hovered = WindowAt(Cursor.Position);
        status.Text = hovered == null ? Tr("Drag the crosshair to another window, then release.", "将十字准星拖到其他窗口，松开后绑定。")
            : Tr("Release to bind: ", "松开后绑定：") + hovered.Title;
    }

    private void FinishTargetPick(object sender, MouseEventArgs args)
    {
        if (!pickingTarget || args.Button != MouseButtons.Left) return;
        // Resolve the screen position before releasing mouse capture.
        TargetWindow selected = WindowAt(Cursor.Position);
        pickingTarget = false;
        targetPicker.Capture = false;
        if (selected == null)
        {
            status.Text = Tr("No usable window selected; the previous target remains bound.", "未选中可用窗口，原绑定保持不变。");
            return;
        }
        target = selected;
        targetLabel.Text = Tr("Target: ", "目标：") + selected.Title;
        targetTip.SetToolTip(targetLabel, selected.Title);
        status.Text = Tr("Bound to: ", "已绑定：") + selected.Title;
    }

    private void CancelLostTargetPick(object sender, EventArgs args)
    {
        if (pickingTarget && !targetPicker.Capture)
        {
            pickingTarget = false;
            status.Text = Tr("Drag canceled; the previous target remains bound.", "拖动已取消，原绑定保持不变。");
        }
    }

    private static TargetWindow WindowAt(Point screenPoint)
    {
        IntPtr child = Native.WindowFromPoint(screenPoint);
        if (child == IntPtr.Zero) return null;
        IntPtr window = Native.GetAncestor(child, 2); // GA_ROOT: bind the top-level window.
        if (window == IntPtr.Zero || !Native.IsWindowVisible(window)) return null;
        uint processId;
        if (Native.GetWindowThreadProcessId(window, out processId) == 0 ||
            processId == (uint)System.Diagnostics.Process.GetCurrentProcess().Id) return null;
        int length = Native.GetWindowTextLength(window);
        if (length <= 0) return null;
        StringBuilder title = new StringBuilder(length + 1);
        Native.GetWindowText(window, title, title.Capacity);
        string caption = title.ToString().Replace('\r', ' ').Replace('\n', ' ').Replace('\t', ' ');
        return caption.Length == 0 ? null : new TargetWindow { Handle = window, ProcessId = processId, Title = caption };
    }

    private void SendRequested()
    {
        if (autoRunning) StopAuto(true);
        else if (autoMode.Checked) StartAuto();
        else SendCurrentLine();
    }

    private void SendCurrentLine()
    {
        if (busy || autoRunning) return;
        int number = editor.GetLineFromCharIndex(editor.SelectionStart);
        string[] lines = editor.Lines;
        string command = number < lines.Length ? lines[number] : "";
        if (command.Length > 4096 || HasControlCharacters(command))
        {
            MessageBox.Show(this, Tr("A line may contain at most 4096 characters and no tabs or control characters.", "单行最多 4096 字符，且不能包含制表符或控制字符。"), Tr("Cannot Send", "无法发送"));
            return;
        }
        if (target == null || !target.IsValid())
        {
            MessageBox.Show(this, Tr("The target is not bound or has closed. Bind it again.", "目标窗口未绑定或已关闭，请重新绑定。"), Tr("Cannot Send", "无法发送"));
            return;
        }
        TargetWindow destination = target;
        if (Native.IsIconic(destination.Handle)) Native.ShowWindow(destination.Handle, 9);
        Native.SetForegroundWindow(destination.Handle);
        busy = true;
        sendButton.Enabled = false;
        Timer timer = new Timer();
        timer.Interval = 250;
        timer.Tick += delegate
        {
            timer.Stop();
            timer.Dispose();
            try
            {
                if (!destination.IsValid() || Native.GetForegroundWindow() != destination.Handle)
                    throw new InvalidOperationException(Tr("The target window did not gain focus. Sending was canceled. Select the terminal input area and bind again if needed.", "目标窗口未获得焦点，已取消发送。请重新绑定并确保终端输入区域处于激活状态。"));
                if (command.Length > 0) SendUnicode(command);
                SendKey(13);
                AdvanceToNextLine(number);
                status.Text = SentStatus(number);
            }
            catch (Exception error)
            {
                MessageBox.Show(this, error.Message, Tr("Send Failed", "发送失败"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally { busy = false; sendButton.Enabled = true; }
        };
        timer.Start();
    }

    private void StartAuto()
    {
        if (busy || autoRunning) return;
        string[] lines = editor.Lines;
        if (rangeStart < 0 || rangeEnd < 0 || rangeStart > rangeEnd || rangeEnd >= lines.Length)
        {
            MessageBox.Show(this, Tr("Right-click the line numbers to set a valid start and end line (both included).", "请在左侧行号上点击右键，设置有效的起点和终点（包含两端）。"), Tr("Auto Send", "自动发送"));
            return;
        }
        if (target == null || !target.IsValid())
        {
            MessageBox.Show(this, Tr("Bind a usable target window first.", "请先绑定可用的目标窗口。"), Tr("Auto Send", "自动发送"));
            return;
        }
        string[] commands = new string[rangeEnd - rangeStart + 1];
        for (int line = rangeStart; line <= rangeEnd; line++)
        {
            string command = lines[line];
            if (command.Length > 4096 || HasControlCharacters(command))
            {
                MessageBox.Show(this, Tr("Line " + (line + 1) + " exceeds 4096 characters or contains a control character.", "第" + (line + 1) + "行超过 4096 字符或包含控制字符，无法自动发送。"), Tr("Auto Send", "自动发送"));
                return;
            }
            commands[line - rangeStart] = command;
        }
        autoCommands = commands;
        autoTarget = target;
        autoNextLine = rangeStart;
        autoEndLine = rangeEnd;
        autoRunning = true;
        editor.ReadOnly = true;
        autoMode.Enabled = false;
        targetPicker.Enabled = false;
        intervalSeconds.Enabled = false;
        UpdateSendAction();
        status.Text = Tr("Sending line " + (rangeStart + 1) + " shortly", "即将发送第" + (rangeStart + 1) + "行");
        if (Native.IsIconic(autoTarget.Handle)) Native.ShowWindow(autoTarget.Handle, 9);
        Native.SetForegroundWindow(autoTarget.Handle);
        autoTimer.Interval = 250;
        autoTimer.Start();
    }

    private void SendAutoLine()
    {
        autoTimer.Stop();
        if (!autoRunning) return;
        int line = autoNextLine;
        try
        {
            if (!autoTarget.IsValid() || Native.GetForegroundWindow() != autoTarget.Handle)
                throw new InvalidOperationException(Tr("The target window lost focus. Auto send stopped. Check the terminal input area.", "目标窗口未获得焦点，自动发送已停止。请检查终端输入区域。"));
            string command = autoCommands[line - rangeStart];
            if (command.Length > 0) SendUnicode(command);
            SendKey(13);
            AdvanceToNextLine(line);
            status.Text = SentStatus(line);
            autoNextLine++;
            if (autoNextLine > autoEndLine) { StopAuto(false); return; }
            autoTimer.Interval = (int)(intervalSeconds.Value * 1000M);
            autoTimer.Start();
        }
        catch (Exception error)
        {
            StopAuto(false);
            status.Text = Tr("Auto send stopped at line " + (line + 1), "自动发送在第" + (line + 1) + "行停止");
            MessageBox.Show(this, error.Message, Tr("Auto Send Failed", "自动发送失败"), MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void StopAuto(bool userRequested)
    {
        if (!autoRunning) return;
        autoTimer.Stop();
        autoRunning = false;
        autoCommands = null;
        autoTarget = null;
        editor.ReadOnly = false;
        autoMode.Enabled = true;
        targetPicker.Enabled = true;
        intervalSeconds.Enabled = true;
        UpdateSendAction();
        if (userRequested) status.Text = Tr("Auto send stopped", "已停止自动发送");
    }

    private static bool HasControlCharacters(string text)
    {
        foreach (char c in text) if (Char.IsControl(c)) return true;
        return false;
    }

    private void SendUnicode(string text)
    {
        // Keep each SendInput call small while preserving each UTF-16 code unit's key-down/up pair.
        for (int offset = 0; offset < text.Length; offset += 128)
        {
            int size = Math.Min(128, text.Length - offset);
            Native.Input[] inputs = new Native.Input[size * 2];
            for (int i = 0; i < size; i++)
            {
                ushort unit = text[offset + i];
                inputs[2 * i] = Native.Key(0, unit, 4);
                inputs[2 * i + 1] = Native.Key(0, unit, 6);
            }
            if (Native.SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(Native.Input))) != inputs.Length)
                throw new InvalidOperationException(Tr("Keyboard input was not fully submitted. Check the terminal; an administrator terminal requires this app to run with matching privileges.", "键盘输入未完整提交。请检查终端内容；管理员终端需要本工具使用相同权限。"));
        }
    }

    private void SendKey(ushort key)
    {
        Native.Input[] inputs = { Native.Key(key, 0, 0), Native.Key(key, 0, 2) };
        if (Native.SendInput(2, inputs, Marshal.SizeOf(typeof(Native.Input))) != 2)
            throw new InvalidOperationException(Tr("Enter was not submitted. Check the terminal before trying again.", "回车未成功提交。请检查终端内容后再操作。"));
    }

    private void OnClosing(object sender, FormClosingEventArgs args)
    {
        if (autoRunning) StopAuto(true);
        if (!ConfirmDiscard()) args.Cancel = true;
    }

    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new SendToCmd());
    }
}
