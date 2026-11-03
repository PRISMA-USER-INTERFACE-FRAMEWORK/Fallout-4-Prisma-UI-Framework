# Ultralight AppCore 1.4 shader sources

The HLSL files in this directory are the stock D3D11 shader sources from Ultralight AppCore branch `1.4-dev`, used by Prisma_Ultralight to implement the exact Ultralight 1.4 `GPUDriver` Fill and FillPath contracts.

Upstream: https://github.com/ultralight-ux/AppCore/tree/1.4-dev/shaders/hlsl

Upstream shader blobs used by this branch:
- `vs/v2f_c4f_t2f.hlsl`: `7b5a62b76e47e2ce66bce1a5a2616b85132cb664`
- `vs/v2f_c4f_t2f_t2f_d28f.hlsl`: `2c26d71b4e8e661876cdb86463dd382f3d80d7e0`
- `ps/fill_path.hlsl`: `7f1189f5eeed3383df689f1dff1e9505a38dffa3`
- `ps/fill.hlsl`: `c87ad1dd89b028420ffa3be535133604d747708f`

These files remain separately licensed under GNU LGPL-2.1 as distributed by AppCore. `LICENSE-AppCore-LGPL-2.1.txt` contains the complete license text. The `sdEllipse` portions also retain their embedded MIT notice from Inigo Quilez.

Prisma's surrounding D3D11 integration code is separate from these third-party shader sources.
