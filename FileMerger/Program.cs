using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FileMerger
{
    class Program
    {
        static void Main(string[] args)
        {
            string[] files = Directory.GetFiles(@"C:\Users\ben\Desktop\output");
            using (FileStream fs = File.OpenWrite("C:/users/ben/desktop/output/merged.audio"))
            {
                foreach (string file in files)
                {
                    byte[] bytes = File.ReadAllBytes(file);
                    fs.Write(bytes, 0, bytes.Length); 
                }
            }
        }           
    }
}
