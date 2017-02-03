using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace NewLedTester
{
    public partial class HelpWindow : Form
    {
        public static HelpWindow singleton = null;

        public static void Open()
        {
            if (singleton == null)
                singleton = new HelpWindow();

            singleton.Show();
        }

        public HelpWindow()
        {
            InitializeComponent();
        }

        private void HelpWindow_Load(object sender, EventArgs e)
        {
            webBrowser1.Navigate("file:///" + Path.Combine(Program.programDir, "Help.htm"));
            singleton = this;
        }

        private void HelpWindow_Resize(object sender, EventArgs e)
        {
            webBrowser1.Width = ClientRectangle.Width;
            webBrowser1.Height = ClientRectangle.Height;
        }

        private void HelpWindow_FormClosed(object sender, FormClosedEventArgs e)
        {
            singleton = null;
        }

    }
}
