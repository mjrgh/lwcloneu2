using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NewLedTester
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            try
            {
                programDir = AppDomain.CurrentDomain.BaseDirectory;

                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new Form1());

                Application.Exit();
            }
            catch (Exception exc)
            {
                MessageBox.Show(exc.Message);
            }
        }

        public static String programDir;
    }
}
