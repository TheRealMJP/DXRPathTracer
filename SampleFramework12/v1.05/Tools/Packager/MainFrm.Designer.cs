namespace Packager
{
    partial class MainFrm
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
            if(disposing && (components != null))
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
            this.packageButton = new System.Windows.Forms.Button();
            this.sourceTextBox = new System.Windows.Forms.TextBox();
            this.browseProjectButton = new System.Windows.Forms.Button();
            this.includeExeCB = new System.Windows.Forms.CheckBox();
            this.includeCodeCB = new System.Windows.Forms.CheckBox();
            this.includeShaderCacheCB = new System.Windows.Forms.CheckBox();
            this.outputTextBox = new System.Windows.Forms.TextBox();
            this.browseOutputButton = new System.Windows.Forms.Button();
            this.projectLabel = new System.Windows.Forms.Label();
            this.outputLabel = new System.Windows.Forms.Label();
            this.logListBox = new System.Windows.Forms.ListBox();
            this.createZipCB = new System.Windows.Forms.CheckBox();
            this.zipPathTextBox = new System.Windows.Forms.TextBox();
            this.browseZipButton = new System.Windows.Forms.Button();
            this.zipLabel = new System.Windows.Forms.Label();
            this.deleteOutputDirCB = new System.Windows.Forms.CheckBox();
            this.copyGitIgnoreCB = new System.Windows.Forms.CheckBox();
            this.SuspendLayout();
            // 
            // packageButton
            // 
            this.packageButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.packageButton.Location = new System.Drawing.Point(406, 465);
            this.packageButton.Name = "packageButton";
            this.packageButton.Size = new System.Drawing.Size(101, 31);
            this.packageButton.TabIndex = 0;
            this.packageButton.Text = "Package";
            this.packageButton.UseVisualStyleBackColor = true;
            this.packageButton.Click += new System.EventHandler(this.packageButton_Click);
            // 
            // sourceTextBox
            // 
            this.sourceTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.sourceTextBox.Location = new System.Drawing.Point(59, 13);
            this.sourceTextBox.Name = "sourceTextBox";
            this.sourceTextBox.Size = new System.Drawing.Size(403, 20);
            this.sourceTextBox.TabIndex = 1;
            // 
            // browseProjectButton
            // 
            this.browseProjectButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.browseProjectButton.Location = new System.Drawing.Point(468, 11);
            this.browseProjectButton.Name = "browseProjectButton";
            this.browseProjectButton.Size = new System.Drawing.Size(39, 23);
            this.browseProjectButton.TabIndex = 2;
            this.browseProjectButton.Text = "...";
            this.browseProjectButton.UseVisualStyleBackColor = true;
            this.browseProjectButton.Click += new System.EventHandler(this.browseProjectButton_Click);
            // 
            // includeExeCB
            // 
            this.includeExeCB.AutoSize = true;
            this.includeExeCB.Checked = true;
            this.includeExeCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.includeExeCB.Location = new System.Drawing.Point(12, 91);
            this.includeExeCB.Name = "includeExeCB";
            this.includeExeCB.Size = new System.Drawing.Size(85, 17);
            this.includeExeCB.TabIndex = 4;
            this.includeExeCB.Text = "Include EXE";
            this.includeExeCB.UseVisualStyleBackColor = true;
            // 
            // includeCodeCB
            // 
            this.includeCodeCB.AutoSize = true;
            this.includeCodeCB.Checked = true;
            this.includeCodeCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.includeCodeCB.Location = new System.Drawing.Point(13, 114);
            this.includeCodeCB.Name = "includeCodeCB";
            this.includeCodeCB.Size = new System.Drawing.Size(89, 17);
            this.includeCodeCB.TabIndex = 5;
            this.includeCodeCB.Text = "Include Code";
            this.includeCodeCB.UseVisualStyleBackColor = true;
            this.includeCodeCB.CheckedChanged += new System.EventHandler(this.includeCodeCB_CheckedChanged);
            // 
            // includeShaderCacheCB
            // 
            this.includeShaderCacheCB.AutoSize = true;
            this.includeShaderCacheCB.Checked = true;
            this.includeShaderCacheCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.includeShaderCacheCB.Location = new System.Drawing.Point(103, 91);
            this.includeShaderCacheCB.Name = "includeShaderCacheCB";
            this.includeShaderCacheCB.Size = new System.Drawing.Size(132, 17);
            this.includeShaderCacheCB.TabIndex = 6;
            this.includeShaderCacheCB.Text = "Include Shader Cache";
            this.includeShaderCacheCB.UseVisualStyleBackColor = true;
            // 
            // outputTextBox
            // 
            this.outputTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.outputTextBox.Location = new System.Drawing.Point(59, 39);
            this.outputTextBox.Name = "outputTextBox";
            this.outputTextBox.Size = new System.Drawing.Size(403, 20);
            this.outputTextBox.TabIndex = 7;
            // 
            // browseOutputButton
            // 
            this.browseOutputButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.browseOutputButton.Location = new System.Drawing.Point(468, 37);
            this.browseOutputButton.Name = "browseOutputButton";
            this.browseOutputButton.Size = new System.Drawing.Size(39, 23);
            this.browseOutputButton.TabIndex = 8;
            this.browseOutputButton.Text = "...";
            this.browseOutputButton.UseVisualStyleBackColor = true;
            this.browseOutputButton.Click += new System.EventHandler(this.browseOutputButton_Click);
            // 
            // projectLabel
            // 
            this.projectLabel.AutoSize = true;
            this.projectLabel.Location = new System.Drawing.Point(13, 16);
            this.projectLabel.Name = "projectLabel";
            this.projectLabel.Size = new System.Drawing.Size(40, 13);
            this.projectLabel.TabIndex = 9;
            this.projectLabel.Text = "Project";
            // 
            // outputLabel
            // 
            this.outputLabel.AutoSize = true;
            this.outputLabel.Location = new System.Drawing.Point(14, 42);
            this.outputLabel.Name = "outputLabel";
            this.outputLabel.Size = new System.Drawing.Size(39, 13);
            this.outputLabel.TabIndex = 10;
            this.outputLabel.Text = "Output";
            // 
            // logListBox
            // 
            this.logListBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.logListBox.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.logListBox.FormattingEnabled = true;
            this.logListBox.ItemHeight = 14;
            this.logListBox.Location = new System.Drawing.Point(12, 137);
            this.logListBox.Name = "logListBox";
            this.logListBox.Size = new System.Drawing.Size(494, 312);
            this.logListBox.TabIndex = 11;
            // 
            // createZipCB
            // 
            this.createZipCB.AutoSize = true;
            this.createZipCB.Checked = true;
            this.createZipCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.createZipCB.Location = new System.Drawing.Point(103, 114);
            this.createZipCB.Name = "createZipCB";
            this.createZipCB.Size = new System.Drawing.Size(96, 17);
            this.createZipCB.TabIndex = 12;
            this.createZipCB.Text = "Create ZIP File";
            this.createZipCB.UseVisualStyleBackColor = true;
            this.createZipCB.CheckedChanged += new System.EventHandler(this.createZipCB_CheckedChanged);
            // 
            // zipPathTextBox
            // 
            this.zipPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.zipPathTextBox.Location = new System.Drawing.Point(59, 65);
            this.zipPathTextBox.Name = "zipPathTextBox";
            this.zipPathTextBox.Size = new System.Drawing.Size(403, 20);
            this.zipPathTextBox.TabIndex = 13;
            // 
            // browseZipButton
            // 
            this.browseZipButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.browseZipButton.Location = new System.Drawing.Point(468, 63);
            this.browseZipButton.Name = "browseZipButton";
            this.browseZipButton.Size = new System.Drawing.Size(39, 23);
            this.browseZipButton.TabIndex = 14;
            this.browseZipButton.Text = "...";
            this.browseZipButton.UseVisualStyleBackColor = true;
            this.browseZipButton.Click += new System.EventHandler(this.browseZipButton_Click);
            // 
            // zipLabel
            // 
            this.zipLabel.AutoSize = true;
            this.zipLabel.Location = new System.Drawing.Point(25, 67);
            this.zipLabel.Name = "zipLabel";
            this.zipLabel.Size = new System.Drawing.Size(24, 13);
            this.zipLabel.TabIndex = 15;
            this.zipLabel.Text = "ZIP";
            // 
            // deleteOutputDirCB
            // 
            this.deleteOutputDirCB.AutoSize = true;
            this.deleteOutputDirCB.Checked = true;
            this.deleteOutputDirCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.deleteOutputDirCB.Location = new System.Drawing.Point(241, 91);
            this.deleteOutputDirCB.Name = "deleteOutputDirCB";
            this.deleteOutputDirCB.Size = new System.Drawing.Size(137, 17);
            this.deleteOutputDirCB.TabIndex = 16;
            this.deleteOutputDirCB.Text = "Delete Output Directory";
            this.deleteOutputDirCB.UseVisualStyleBackColor = true;
            // 
            // copyGitIgnoreCB
            // 
            this.copyGitIgnoreCB.AutoSize = true;
            this.copyGitIgnoreCB.Checked = true;
            this.copyGitIgnoreCB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.copyGitIgnoreCB.Location = new System.Drawing.Point(241, 114);
            this.copyGitIgnoreCB.Name = "copyGitIgnoreCB";
            this.copyGitIgnoreCB.Size = new System.Drawing.Size(120, 17);
            this.copyGitIgnoreCB.TabIndex = 17;
            this.copyGitIgnoreCB.Text = "Copy .gitignore Files";
            this.copyGitIgnoreCB.UseVisualStyleBackColor = true;
            // 
            // MainFrm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(519, 508);
            this.Controls.Add(this.copyGitIgnoreCB);
            this.Controls.Add(this.deleteOutputDirCB);
            this.Controls.Add(this.zipLabel);
            this.Controls.Add(this.browseZipButton);
            this.Controls.Add(this.zipPathTextBox);
            this.Controls.Add(this.createZipCB);
            this.Controls.Add(this.logListBox);
            this.Controls.Add(this.outputLabel);
            this.Controls.Add(this.projectLabel);
            this.Controls.Add(this.browseOutputButton);
            this.Controls.Add(this.outputTextBox);
            this.Controls.Add(this.includeShaderCacheCB);
            this.Controls.Add(this.includeCodeCB);
            this.Controls.Add(this.includeExeCB);
            this.Controls.Add(this.browseProjectButton);
            this.Controls.Add(this.sourceTextBox);
            this.Controls.Add(this.packageButton);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "MainFrm";
            this.Text = "Packager";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button packageButton;
        private System.Windows.Forms.TextBox sourceTextBox;
        private System.Windows.Forms.Button browseProjectButton;
        private System.Windows.Forms.CheckBox includeExeCB;
        private System.Windows.Forms.CheckBox includeCodeCB;
        private System.Windows.Forms.CheckBox includeShaderCacheCB;
        private System.Windows.Forms.TextBox outputTextBox;
        private System.Windows.Forms.Button browseOutputButton;
        private System.Windows.Forms.Label projectLabel;
        private System.Windows.Forms.Label outputLabel;
        private System.Windows.Forms.ListBox logListBox;
        private System.Windows.Forms.CheckBox createZipCB;
        private System.Windows.Forms.TextBox zipPathTextBox;
        private System.Windows.Forms.Button browseZipButton;
        private System.Windows.Forms.Label zipLabel;
        private System.Windows.Forms.CheckBox deleteOutputDirCB;
        private System.Windows.Forms.CheckBox copyGitIgnoreCB;
    }
}

