using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading.Tasks;

namespace HelperComponent
{
    public sealed class Helper
    {
        
        public static string ConvertToString([ReadOnlyArray] byte[] bytes)
        {
            return Convert.ToBase64String(bytes);
        }
    }
}
