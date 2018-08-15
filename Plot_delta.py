import numpy as np
import matplotlib.pyplot as plt 
from scipy.interpolate import interp1d
from matplotlib.backends.backend_pdf import PdfPages


file_name_1 = 'output/L_background.dat'
file_name_2 = 'output/Planck_bestfit01_background.dat'


# gdm = np.loadtxt('output/Added_GDM09_background.dat')
# cdm = np.loadtxt('output/Added_GDM05_background.dat')
# ceff_cvis_0p3 = np.loadtxt('output/Added_GDM03_perturbations_k0_s.dat')

file_1 = np.loadtxt(file_name_1)
file_2 = np.loadtxt(file_name_2)

index_x1 = 0
index_y1 = 3
index_x2 = 0
index_y2 = 3


x_1 = file_1[:,index_x1]
y_1 = interp1d(x_1, file_1[:,index_y1])

x_2 = file_2[:,index_x2]
y_2 = interp1d(x_2, file_2[:,index_y2])


plt.plot(x_1, y_1(x_1), label = 'file_1')
plt.plot(x_2, y_2(x_2), label = 'file_2')

plt.xscale('log')
plt.yscale('log')
plt.legend()
# plt.xlabel(r'$\ell$', fontsize = 18)
# plt.ylabel(r'$\left(\frac{C_{\ell}^{GDM} - C_{\ell}^{CDM}}{C_{\ell}^{GDM}}\right)^{TT, lensed}$', fontsize = 18)
# plt.tight_layout()
# plt.title('Ratio of GDM to CDM in the TT poswer spectrum')

# plot_name = 'GDM_CDM_ratio_TT_unlensed.pdf'

# with PdfPages(plot_name) as pdf:
# 	pdf.savefig(bbox_inches='tight')

plt.show()



plt.plot(x_1, y_2(x_1)/y_1(x_1) - 1, label = 'file_2 / file_1 - 1')

plt.xscale('log')
plt.legend()

# plt.xlabel(r'$\ell$', fontsize = 18)
# plt.ylabel(r'$\left(\frac{C_{\ell}^{GDM} - C_{\ell}^{CDM}}{C_{\ell}^{GDM}}\right)^{TT, lensed}$', fontsize = 18)
# plt.tight_layout()
# plt.title('Ratio of GDM to CDM in the TT poswer spectrum')

# plot_name = 'GDM_CDM_ratio_TT_unlensed.pdf'

# with PdfPages(plot_name) as pdf:
# 	pdf.savefig(bbox_inches='tight')


plt.show()