using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Windows.Input;

namespace NewLedTester
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
            dll = new LedWizDLL();
        }

        LedWizDLL dll;
        bool hwndRegistered = false;

        private void Form1_Resize(object sender, EventArgs e)
        {
            int w = ClientRectangle.Width, h = ClientRectangle.Height;
            panel1.Height = h + 2;
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            dll.LWZ_SET_NOTIFY(LWNotify, ref lwList);

            OrientedTextLabel l = new OrientedTextLabel();
            l.Text = "Brightness";
            l.TextOrientation = Orientation.Rotate;
            l.Width = 20;
            l.Height = tbBri.Height;
            l.RotationAngle = 270;
            l.Left = tbBri.Left - l.Width;
            l.Top = tbBri.Top;
            l.TextAlign = ContentAlignment.MiddleCenter;
            Controls.Add(l);
        }

        class Device
        {
            // add a new device to the UI
            public static void Add(Form1 form, uint unit, uint deviceType, String name)
            {
                // create the panel for the new LedWiz
                Panel p = new Panel();
                p.Width = form.panel1.Width - 20;

                // add the device object
                Device newDev = new Device(form, unit, p);

                // Figure the base port.  For ordinary LedWiz units, the ports are
                // numbered 1-32.  If it's a Pinscape virtual LedWiz, though, it represents
                // a group of high-numbered ports, which we can get from the name.
                int basePort = 1;
                if (deviceType == LedWizDLL.LWZ_DEVICE_TYPE_PINSCAPE_VIRT
                    || deviceType == LedWizDLL.LWZ_DEVICE_TYPE_PINSCAPE_PICO)
                {
                    Match m = Regex.Match(name, @"(?i)ports\s+(\d+)-(\d+)");
                    if (m.Success)
                        basePort = int.Parse(m.Groups[1].Value);
                }

                // add the device name
                Label l = new Label();
                l.Text = "LedWiz #" + unit;
                l.Font = new Font("Arial", SystemFonts.DefaultFont.SizeInPoints, FontStyle.Bold);
                l.AutoSize = true;
                l.Top = 4;
                l.Left = 4;
                p.Controls.Add(l);

                if (name != null)
                {
                    Label l2 = new Label();
                    l2.Text = name;
                    l2.Font = new Font("Arial", SystemFonts.DefaultFont.SizeInPoints, FontStyle.Italic);
                    l2.AutoSize = true;
                    l2.Top = l.Top;
                    l2.Left = l.Right + 8;
                    p.Controls.Add(l2);
                }

                // checkbox event handlers
                int lastButtonClicked = 0;
                Action<int, CheckBox> setCkEvents = (int idx, CheckBox eck) =>
                {
                    eck.MouseDown += (object sender, System.Windows.Forms.MouseEventArgs e) =>
                    {
                        // note the shift and control key status
                        bool shift = Keyboard.IsKeyDown(Key.LeftShift) || Keyboard.IsKeyDown(Key.RightShift);
                        bool ctrl = Keyboard.IsKeyDown(Key.LeftCtrl) || Keyboard.IsKeyDown(Key.RightCtrl);

                        // If the Ctrl key is down, we keep the current selection,
                        // otherwise we clear the selection
                        if (!ctrl)
                            form.SelectNone();

                        // If the Shift key is down, select items from the last clicked
                        // button to here, inclusive.  Otherwise, if the Ctrl key is down,
                        // invert the selection status of the current item.  Otherwise
                        // select the current item.
                        if (shift)
                        {
                            // Shift+Click - select items from the previous item to here
                            int a = lastButtonClicked;
                            int b = idx;
                            if (a > b)
                            {
                                int c = a;
                                a = b;
                                b = c;
                            }
                            for (int i = a; i <= b; ++i)
                                newDev.portButtons[i].Checked = true;
                        }
                        else if (ctrl)
                        {
                            // Ctrl+Click - invert the currente item's selection state
                            eck.Checked = !eck.Checked;

                            // this is now the last button clicked
                            lastButtonClicked = idx;
                        }
                        else
                        {
                            // plain click - just select the current item
                            eck.Checked = true;

                            // this is now the last button clicked
                            lastButtonClicked = idx;
                        }

                        // update the selection
                        form.UpdateSelection();
                    };
                    eck.CheckedChanged += (object sender, EventArgs e) => {
                        eck.BackColor = eck.Checked ? Color.Yellow : SystemColors.Control;
                    };
                };

                // add the port selector buttons
                for (int i = 0 ; i < 32 ; ++i)
                {
                    // set up a button-style checkbox labeled with the port number
                    CheckBox ck = new CheckBox();
                    ck.Appearance = Appearance.Button;
                    ck.AutoCheck = false;
                    ck.Height = 29;
                    ck.Width = 32;
                    ck.Left = 8 + (ck.Width-2) * (i % 16);
                    ck.Top = 24 + (ck.Height-2) * (i / 16);
                    ck.Text = (basePort + i).ToString();
                    ck.TextAlign = ContentAlignment.MiddleCenter;
                    p.Controls.Add(ck);
                    newDev.portButtons[i] = ck;

                    // add the events for the button
                    setCkEvents(i, ck);
                }

#if false
                // Add Select All and Select None buttons.  I'm leaving these out,
                // since they seem unnecessary given the versatility of the various
                // Shift+Click and Ctrl+Click modes, but I'll leave the code in case
                // they become desirable in the future.
                Button bAll = new Button();
                bAll.Text = "Select All";
                bAll.AutoSize = true;
                bAll.Top = p.Controls[p.Controls.Count - 1].Bottom + 4;
                bAll.Left = 8;
                bAll.Click += (object sender, EventArgs e) =>
                {
                    newDev.SelectAll();
                    form.UpdateSelection();
                };
                p.Controls.Add(bAll);

                Button bNone = new Button();
                bNone.Text = "Select None";
                bNone.AutoSize = true;
                bNone.Top = bAll.Top;
                bNone.Left = bAll.Right + 4;
                bNone.Click += (object sender, EventArgs e) =>
                {
                    newDev.SelectNone();
                    form.UpdateSelection();
                };
                p.Controls.Add(bNone);
#endif

                // find the height of the contents
                int ymax = 0;
                foreach (Control chi in p.Controls)
                {
                    if (chi.Bottom > ymax)
                        ymax = chi.Bottom;
                }

                // add a separator bar
                Label bar = new Label();
                bar.BorderStyle = BorderStyle.FixedSingle;
                bar.Height = 1;
                bar.Width = p.Width - 8;
                bar.Left = 4;
                bar.Top = ymax + 8;
                p.Controls.Add(bar);
                newDev.bottomBar = bar;

                // the bar is at the bottom of the panel
                ymax = bar.Top + 2;

                // set the panel height according to the contents
                p.Height = ymax + 4;
                int y = 0;
				foreach (Device dev in form.devices)
                {
                    if (dev.unit < unit && dev.panel.Bottom > y)
                        y = dev.panel.Bottom;
                }

                // move following units down one notch
                foreach (Device dev in form.devices)
                {
                    if (dev.unit > unit)
                        dev.panel.Top += p.Height;
                }

                // insert the new panel
                p.Top = y;
                form.panel1.Controls.Add(p);

                // insert the new device object
                form.devices.Add(newDev);

                // sort by unit number
                form.devices.Sort((a, b) => (int)a.unit - (int)b.unit);

                // show the bar in all but the last panel
                for (int i = 0 ; i < form.devices.Count ; ++i)
                    form.devices[i].bottomBar.Visible = (i+1 < form.devices.Count);

                // Turn everything off on the physical unit so that it matches
                // our initial internal state.  (If the LedWiz API had a way to
                // query the device's state, we could instead set our initial
                // state to match the physical device state, but the API has 
                // no such capability, so syncing can only be host-to-device.)
                newDev.AllOff();
            }

            Device(Form1 form, UInt32 unit, Panel panel)
            {
                this.form = form;
                this.unit = unit;
                this.panel = panel;
                for (int i = 0; i < 32; ++i)
                    bri[i] = 48;
            }

            Form1 form;
            public UInt32 unit;
            public Panel panel;
            Label bottomBar;

            // port states
            bool[] on = new bool[32];
            int[] bri = new int[32];
            int speed = 2;

            // port buttons
            CheckBox[] portButtons = new CheckBox[32];

            // set all selected buttons to a new state
            public void ApplyControlChanges(String on, String bri, String speed)
            {
                int newOn = -1;
                int newBri = -1;

                switch (on)
                {
                    case "On":
                        newOn = 1;
                        break;

                    case "Off":
                        newOn = 0;
                        break;
                }

                Match m;
                switch (bri)
                {
                    case "Sawtooth":
                        newBri = 129;
                        break;

                    case "Square Wave":
                        newBri = 130;
                        break;

                    case "On/Down":
                        newBri = 131;
                        break;

                    case "Up/On":
                        newBri = 132;
                        break;

                    default:
                        m = Regex.Match(bri, @"Brightness: (\d+)");
                        if (m.Success)
                            newBri = int.Parse(m.Groups[1].Value);
                        break;
                }

                // apply the change to each selected control
                int selectionCount = 0;
                for (int i = 0; i < 32; ++i)
                {
                    CheckBox ck = portButtons[i];
                    if (ck.Checked)
                    {
                        selectionCount++;
                        if (newOn != -1)
                            this.on[i] = (newOn == 1);
                        if (newBri != -1)
                            this.bri[i] = newBri;
                    }
                }

                // only apply the speed change if at least one checkbox is selected
                if (selectionCount != 0)
                {
                    m = Regex.Match(speed, @"Speed: (\d+)");
                    if (m.Success)
                        this.speed = int.Parse(m.Groups[1].Value);
                }

                form.dll.LWZ_SBA(unit, SBA(0), SBA(8), SBA(16), SBA(24), (uint)this.speed);
                form.dll.LWZ_PBA(unit, PBA());
            }

            public void UpdateSelection(ref int groupOn, ref int groupBri, ref int groupSpeed)
            {
                // get the selected ports
                List<int> selectedPorts = GetSelection();

                // check each state
                foreach (int port in selectedPorts)
                {
                    // Check the group 'on' state.  If it's uninitialized, take this 
                    // port value; otherwise, if it doesn't match this port's value, 
                    // we have a mixed selection.
                    if (groupOn == -2)
                        groupOn = on[port] ? 1 : 0;
                    else if ((groupOn == 1) != on[port])
                        groupOn = -1;

                    // Do the same with the brightness state
                    if (groupBri == -2)
                        groupBri = bri[port];
                    else if (groupBri != bri[port])
                        groupBri = -1;
                }

                // if the list is non-empty, do the same with the speed setting
                // for the whole unit
                if (selectedPorts.Count != 0)
                {
                    if (groupSpeed == -2)
                        groupSpeed = speed;
                    else if (groupSpeed != speed)
                        groupSpeed = -1;
                }
            }

            List<int> GetSelection()
            {
                // make a list of selected port selector buttons
                List<int> selected = new List<int>();
                for (int i = 0; i < 32; ++i)
                {
                    if (portButtons[i].Checked)
                        selected.Add(i);
                }

                return selected;
            }

            public void SelectAll()
            {
                for (int i = 0; i < 32; ++i)
                    portButtons[i].Checked = true;
            }

            public void SelectNone()
            {
                for (int i = 0 ; i < 32 ; ++i)
                    portButtons[i].Checked = false;
            }

            public void AllOff()
            {
                // turn all ports off and set the default speed (2)
                form.dll.LWZ_SBA(unit, 0, 0, 0, 0, 2);

                // set all ports to the default brightness of 48
                byte[] b = new byte[32];
                for (int i = 0; i < 32; ++i)
                    b[i] = 48;
                form.dll.LWZ_PBA(unit, b);
            }

            uint SBA(int startPort)
            {
                uint ret = 0;
                for (int i = 0; i < 8; ++i)
                    ret = (ret >> 1) | (uint)(on[startPort + i] ? 0x80 : 0);
                return ret;
            }

            byte[] PBA()
            {
                return bri.Select(b => (byte)b).ToArray();
            }

        }
        List<Device> devices = new List<Device>();

        void AddLedWiz(UInt32 unit)
        {
            // retrieve the unit name, if we have the extended API available
            String name = null;
            uint deviceType = 0;
            if (dll.LWZ_GET_DEVICE_INFO != null)
            {
                LedWizDLL.LWZDEVICEINFO info = new LedWizDLL.LWZDEVICEINFO();
                info.cbSize = (UInt32)Marshal.SizeOf(info.GetType());
                dll.LWZ_GET_DEVICE_INFO(unit, ref info);
                unsafe { name = new String(info.szName); }
                deviceType = info.dwDevType;
            }

            // add the device
            Device.Add(this, unit, deviceType, name);

            // hide the "no units found" label, since we have units now
            panelNoDevices.Visible = false;

            // if we haven't already registered for notifications, do so now
            if (!hwndRegistered)
            {
                dll.LWZ_REGISTER(unit, Handle);
                hwndRegistered = true;
            }
        }

        void DeleteLedWiz(UInt32 unit)
        {
            // see if we can find a match for this device in the list
            Device deldev = devices.FirstOrDefault((d) => d.unit == unit);
            if (deldev != null)
            {
                // found it - move all items below the deleted item up one slot
                foreach (Device dev in devices)
                {
                    if (dev.unit > unit)
                        dev.panel.Top -= dev.panel.Height;
                }

                // remove the deleted item
                devices.Remove(deldev);
                panel1.Controls.Remove(deldev.panel);
            }

            // if there are no units in the list now, show the "no units found" label
            if (devices.Count == 0)
                panelNoDevices.Visible = true;
        }

        LedWizDLL.LWZDEVICELIST lwList = new LedWizDLL.LWZDEVICELIST();
        private void LWNotify(Int32 reason, UInt32 handle)
        {
            switch (reason)
            {
                case LedWizDLL.REASON_ADD:
                    AddLedWiz(handle);
                    break;

                case LedWizDLL.REASON_DELETE:
                    DeleteLedWiz(handle);
                    break;
            }
        }

        unsafe private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            dll.LWZ_UNSET_NOTIFY();
        }

        // Apply control changes to the selected output ports
        void ApplyControlChanges()
        {
            // update the devices
            foreach (Device dev in devices)
                dev.ApplyControlChanges(ckOn.Text, lblBri.Text, lblSpeed.Text);

            // set all controls to reflect the selection state
            UpdateSelection();
        }

        // Deselect all ports across all devices
        void SelectNone()
        {
            foreach (Device dev in devices)
                dev.SelectNone();
        }

        // Set the controls to reflect the state of the current selection
        void UpdateSelection()
        {
            // for each state, -2 = uninitialized, -1 = mixed
            int on = -2;
            int bri = -2;
            int speed = -2;

            // go through all selections on all units to see if we have a
            // single mode selected for each state
            foreach (Device dev in devices)
                dev.UpdateSelection(ref on, ref bri, ref speed);

            // Apply the results to the controls
            SetOnButton(on == -1 ? "Mixed" : on == 1 ? "On" : "Off");
            SetBrightnessBar(bri);
            SetSpeedBar(speed);
        }

        private void ckOn_Click(object sender, EventArgs e)
        {
            SetOnButton(ckOn.Checked ? "On" : "Off");
            ApplyControlChanges();
        }

        private void SetOnButton(String txt)
        {
            ckOn.Text = txt;
            switch (txt)
            {
                case "On":
                    ckOn.BackColor = Color.Yellow;
                    ckOn.Checked = true;
                    break;

                case "Off":
                    ckOn.BackColor = SystemColors.Control;
                    ckOn.Checked = false;
                    break;

                case "Mixed":
                    ckOn.BackColor = Color.LightGray;
                    ckOn.CheckState = CheckState.Indeterminate;
                    break;
            }
        }

        // val = -1 for mixed, 129 for sawtooth, 130 for square wave,
        // 131 for up/on, 132 for on/down
        private void SetBrightnessBar(int val)
        {
            // presume the trackbar is at zero and all the modes are off
            ClearFlashModes();

            // use full brightness unless we find another value
            int newval = 48;

            // see what we have
            CheckBox ckWave = null;
            switch (val)
            {
                case -1:
                    lblBri.Text = "Mixed Modes";
                    break;

                case -2:
                    lblBri.Text = "Brightness: 48";
                    break;

                case 129:
                    lblBri.Text = "Sawtooth";
                    ckWave = ckSawtooth;
                    break;

                case 130:
                    lblBri.Text = "Square Wave";
                    ckWave = ckSquareWave;
                    break;

                case 131:
                    lblBri.Text = "On/Down";
                    ckWave = ckOnDown;
                    break;

                case 132:
                    lblBri.Text = "Up/On";
                    ckWave = ckUpOn;
                    break;

                default:
                    lblBri.Text = "Brightness: " + val;
                    newval = val;
                    break;
            }

            // update the bar
            tbBri.Value = newval;

            // if one of the waveforms is activated, select the control
            if (ckWave != null)
            {
                ckWave.Checked = true;
                ckWave.BackColor = Color.FromArgb(43, 53, 255);
                ckWave.Image = Properties.Resources.ResourceManager.GetObject(ckWave.Name.Substring(2) + "On") as Image;
            }
        }

        private void SetSpeedBar(int val)
        {
            if (val == -1)
            {
                lblSpeed.Text = "Speed: Mixed";
                tbSpeed.Value = 2;
            }
            else if (val == -2)
            {
                lblSpeed.Text = "Speed: 2";
                tbSpeed.Value = 2;
            }
            else
            {
                lblSpeed.Text = "Speed: " + val;
                tbSpeed.Value = val;
            }
        }

        private void tbBri_Scroll(object sender, EventArgs e)
        {
            lblBri.Text = "Brightness: " + tbBri.Value;
            ApplyControlChanges();
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            foreach (Device dev in devices)
                dev.AllOff();
        }

        private void ClearFlashModes()
        {
            foreach (var ck in new CheckBox[] { ckSawtooth, ckSquareWave, ckOnDown, ckUpOn })
            {
                ck.Checked = false;
                ck.BackColor = Color.FromArgb(127, 127, 127);
                ck.Image = Properties.Resources.ResourceManager.GetObject(ck.Name.Substring(2)) as Image;
            }
        }

        private void tbSpeed_Scroll(object sender, EventArgs e)
        {
            lblSpeed.Text = "Speed: " + tbSpeed.Value;
            ApplyControlChanges();
        }

        private void ClickFlashMode(CheckBox ck, String txt)
        {
            ClearFlashModes();
            lblBri.Text = txt;
            ApplyControlChanges();
        }

        private void ckSawtooth_Click(object sender, EventArgs e)
        {
            ClickFlashMode(ckSawtooth, "Sawtooth");
        }

        private void ckSquareWave_Click(object sender, EventArgs e)
        {
            ClickFlashMode(ckSquareWave, "Square Wave");
        }

        private void ckUpOn_Click(object sender, EventArgs e)
        {
            ClickFlashMode(ckUpOn, "Up/On");
        }

        private void ckOnDown_Click(object sender, EventArgs e)
        {
            ClickFlashMode(ckOnDown, "On/Down");
        }

        private void btnHelp_Click(object sender, EventArgs e)
        {
            HelpWindow.Open();
        }

        private void btnRefresh_Click(object sender, EventArgs e)
        {
            dll.Reload();
            dll.LWZ_SET_NOTIFY(LWNotify, ref lwList);
        }
    }
}
