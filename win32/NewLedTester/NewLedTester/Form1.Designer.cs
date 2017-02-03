namespace NewLedTester
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.panel1 = new System.Windows.Forms.Panel();
            this.tbBri = new System.Windows.Forms.TrackBar();
            this.lblBri = new System.Windows.Forms.Label();
            this.ckOn = new System.Windows.Forms.CheckBox();
            this.ckSawtooth = new System.Windows.Forms.CheckBox();
            this.ckSquareWave = new System.Windows.Forms.CheckBox();
            this.ckUpOn = new System.Windows.Forms.CheckBox();
            this.ckOnDown = new System.Windows.Forms.CheckBox();
            this.tbSpeed = new System.Windows.Forms.TrackBar();
            this.lblSpeed = new System.Windows.Forms.Label();
            this.lblNoDevices = new System.Windows.Forms.Label();
            this.btnHelp = new System.Windows.Forms.Button();
            this.btnRefresh = new System.Windows.Forms.Button();
            this.panelNoDevices = new System.Windows.Forms.Panel();
            this.panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.tbBri)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.tbSpeed)).BeginInit();
            this.panelNoDevices.SuspendLayout();
            this.SuspendLayout();
            // 
            // panel1
            // 
            this.panel1.AutoScroll = true;
            this.panel1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.panel1.Controls.Add(this.panelNoDevices);
            this.panel1.Location = new System.Drawing.Point(-1, -1);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(530, 390);
            this.panel1.TabIndex = 0;
            // 
            // tbBri
            // 
            this.tbBri.Location = new System.Drawing.Point(567, 74);
            this.tbBri.Maximum = 48;
            this.tbBri.Minimum = 1;
            this.tbBri.Name = "tbBri";
            this.tbBri.Orientation = System.Windows.Forms.Orientation.Vertical;
            this.tbBri.Size = new System.Drawing.Size(45, 146);
            this.tbBri.TabIndex = 3;
            this.tbBri.TickFrequency = 6;
            this.tbBri.TickStyle = System.Windows.Forms.TickStyle.Both;
            this.tbBri.Value = 48;
            this.tbBri.Scroll += new System.EventHandler(this.tbBri_Scroll);
            // 
            // lblBri
            // 
            this.lblBri.Location = new System.Drawing.Point(539, 46);
            this.lblBri.Name = "lblBri";
            this.lblBri.Size = new System.Drawing.Size(100, 23);
            this.lblBri.TabIndex = 2;
            this.lblBri.Text = "Brightness: 48";
            this.lblBri.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // ckOn
            // 
            this.ckOn.Appearance = System.Windows.Forms.Appearance.Button;
            this.ckOn.Location = new System.Drawing.Point(554, 13);
            this.ckOn.Name = "ckOn";
            this.ckOn.Size = new System.Drawing.Size(70, 24);
            this.ckOn.TabIndex = 1;
            this.ckOn.Text = "Off";
            this.ckOn.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.ckOn.UseVisualStyleBackColor = true;
            this.ckOn.Click += new System.EventHandler(this.ckOn_Click);
            // 
            // ckSawtooth
            // 
            this.ckSawtooth.Appearance = System.Windows.Forms.Appearance.Button;
            this.ckSawtooth.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(127)))), ((int)(((byte)(127)))), ((int)(((byte)(127)))));
            this.ckSawtooth.Image = global::NewLedTester.Properties.Resources.Sawtooth;
            this.ckSawtooth.Location = new System.Drawing.Point(554, 220);
            this.ckSawtooth.Name = "ckSawtooth";
            this.ckSawtooth.Size = new System.Drawing.Size(36, 24);
            this.ckSawtooth.TabIndex = 4;
            this.ckSawtooth.UseVisualStyleBackColor = false;
            this.ckSawtooth.Click += new System.EventHandler(this.ckSawtooth_Click);
            // 
            // ckSquareWave
            // 
            this.ckSquareWave.Appearance = System.Windows.Forms.Appearance.Button;
            this.ckSquareWave.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(127)))), ((int)(((byte)(127)))), ((int)(((byte)(127)))));
            this.ckSquareWave.Image = global::NewLedTester.Properties.Resources.SquareWave;
            this.ckSquareWave.Location = new System.Drawing.Point(587, 220);
            this.ckSquareWave.Name = "ckSquareWave";
            this.ckSquareWave.Size = new System.Drawing.Size(36, 24);
            this.ckSquareWave.TabIndex = 5;
            this.ckSquareWave.UseVisualStyleBackColor = false;
            this.ckSquareWave.Click += new System.EventHandler(this.ckSquareWave_Click);
            // 
            // ckUpOn
            // 
            this.ckUpOn.Appearance = System.Windows.Forms.Appearance.Button;
            this.ckUpOn.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(127)))), ((int)(((byte)(127)))), ((int)(((byte)(127)))));
            this.ckUpOn.Image = global::NewLedTester.Properties.Resources.UpOn;
            this.ckUpOn.Location = new System.Drawing.Point(554, 242);
            this.ckUpOn.Name = "ckUpOn";
            this.ckUpOn.Size = new System.Drawing.Size(36, 24);
            this.ckUpOn.TabIndex = 6;
            this.ckUpOn.UseVisualStyleBackColor = false;
            this.ckUpOn.Click += new System.EventHandler(this.ckUpOn_Click);
            // 
            // ckOnDown
            // 
            this.ckOnDown.Appearance = System.Windows.Forms.Appearance.Button;
            this.ckOnDown.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(127)))), ((int)(((byte)(127)))), ((int)(((byte)(127)))));
            this.ckOnDown.Image = global::NewLedTester.Properties.Resources.OnDown;
            this.ckOnDown.Location = new System.Drawing.Point(587, 242);
            this.ckOnDown.Name = "ckOnDown";
            this.ckOnDown.Size = new System.Drawing.Size(36, 24);
            this.ckOnDown.TabIndex = 7;
            this.ckOnDown.UseVisualStyleBackColor = false;
            this.ckOnDown.Click += new System.EventHandler(this.ckOnDown_Click);
            // 
            // tbSpeed
            // 
            this.tbSpeed.Location = new System.Drawing.Point(549, 304);
            this.tbSpeed.Maximum = 7;
            this.tbSpeed.Minimum = 1;
            this.tbSpeed.Name = "tbSpeed";
            this.tbSpeed.Size = new System.Drawing.Size(78, 45);
            this.tbSpeed.TabIndex = 9;
            this.tbSpeed.Value = 2;
            this.tbSpeed.Scroll += new System.EventHandler(this.tbSpeed_Scroll);
            // 
            // lblSpeed
            // 
            this.lblSpeed.Location = new System.Drawing.Point(542, 278);
            this.lblSpeed.Name = "lblSpeed";
            this.lblSpeed.Size = new System.Drawing.Size(97, 23);
            this.lblSpeed.TabIndex = 8;
            this.lblSpeed.Text = "Speed: 2";
            this.lblSpeed.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // lblNoDevices
            // 
            this.lblNoDevices.AutoSize = true;
            this.lblNoDevices.Location = new System.Drawing.Point(29, 8);
            this.lblNoDevices.Name = "lblNoDevices";
            this.lblNoDevices.Size = new System.Drawing.Size(297, 26);
            this.lblNoDevices.TabIndex = 0;
            this.lblNoDevices.Text = "No LedWiz units have been detected. Please make sure your\r\ndevices are plugged in" +
    " via USB.";
            // 
            // btnHelp
            // 
            this.btnHelp.Location = new System.Drawing.Point(554, 352);
            this.btnHelp.Name = "btnHelp";
            this.btnHelp.Size = new System.Drawing.Size(75, 23);
            this.btnHelp.TabIndex = 1;
            this.btnHelp.Text = "Help";
            this.btnHelp.UseVisualStyleBackColor = true;
            this.btnHelp.Click += new System.EventHandler(this.btnHelp_Click);
            // 
            // btnRefresh
            // 
            this.btnRefresh.Location = new System.Drawing.Point(140, 43);
            this.btnRefresh.Name = "btnRefresh";
            this.btnRefresh.Size = new System.Drawing.Size(75, 23);
            this.btnRefresh.TabIndex = 1;
            this.btnRefresh.Text = "Refresh";
            this.btnRefresh.UseVisualStyleBackColor = true;
            this.btnRefresh.Click += new System.EventHandler(this.btnRefresh_Click);
            // 
            // panelNoDevices
            // 
            this.panelNoDevices.Controls.Add(this.btnRefresh);
            this.panelNoDevices.Controls.Add(this.lblNoDevices);
            this.panelNoDevices.Location = new System.Drawing.Point(87, 13);
            this.panelNoDevices.Name = "panelNoDevices";
            this.panelNoDevices.Size = new System.Drawing.Size(355, 72);
            this.panelNoDevices.TabIndex = 2;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(650, 385);
            this.Controls.Add(this.btnHelp);
            this.Controls.Add(this.lblSpeed);
            this.Controls.Add(this.tbSpeed);
            this.Controls.Add(this.ckOnDown);
            this.Controls.Add(this.ckUpOn);
            this.Controls.Add(this.ckSquareWave);
            this.Controls.Add(this.ckSawtooth);
            this.Controls.Add(this.ckOn);
            this.Controls.Add(this.lblBri);
            this.Controls.Add(this.tbBri);
            this.Controls.Add(this.panel1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Form1";
            this.Text = "New LedWiz Tester";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.Form1_FormClosing);
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Form1_FormClosed);
            this.Load += new System.EventHandler(this.Form1_Load);
            this.Resize += new System.EventHandler(this.Form1_Resize);
            this.panel1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.tbBri)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.tbSpeed)).EndInit();
            this.panelNoDevices.ResumeLayout(false);
            this.panelNoDevices.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.TrackBar tbBri;
        private System.Windows.Forms.Label lblBri;
        private System.Windows.Forms.CheckBox ckOn;
        private System.Windows.Forms.CheckBox ckSawtooth;
        private System.Windows.Forms.CheckBox ckSquareWave;
        private System.Windows.Forms.CheckBox ckUpOn;
        private System.Windows.Forms.CheckBox ckOnDown;
        private System.Windows.Forms.TrackBar tbSpeed;
        private System.Windows.Forms.Label lblSpeed;
        private System.Windows.Forms.Label lblNoDevices;
        private System.Windows.Forms.Button btnHelp;
        private System.Windows.Forms.Button btnRefresh;
        private System.Windows.Forms.Panel panelNoDevices;

    }
}

