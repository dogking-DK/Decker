# test

``` mermaid
flowchart TD
  A[等待第1点] -->[]
  B --> C[等待第2点]
  C -->|LMB| D[记录 p1 (吸附可选)]
  D --> E{两点距离 < δ ?}
  E -- 是 --> E1[提示: 两点太近\n要求重选] --> C
  E -- 否 --> F[计算线方向 a = normalize(p1 - p0)]
  F --> G[取 viewDir = -camera.forward]
  G --> H{|a × viewDir| < ε ?}
  H -- 是 --> H1[选屏幕 right/up 作为备用向量 v'] --> I[ n0 = a × v' ]
  H -- 否 --> I[ n0 = a × viewDir ]
  I --> J{‖n0‖ < ε ?}
  J -- 是 --> J1[提示: 视线近共线\n建议旋转视角] --> C
  J -- 否 --> K[ n = normalize( n0 × a ) ; d = -dot(n, p0) ]
  K --> L[构造平面 Plane(n,d)]
  L --> M[构造局部基 basis (e3=n) 并正交化]
  M --> N[进入预览: 渲染剖切/轮廓]
  N --> O{键/鼠: 偏移/主轴约束/角度步进/法向翻转?}
  O -- 是 --> P[ΔT → 约束管线 → 更新 Plane/offset] --> N
  O -- 否 --> Q{提交/取消?}
  Q -- 提交 --> R[CreateSectionCommand.commit()\n入Undo栈]
  Q -- 取消 --> S[rollback & 清理]
  R --> T[返回 Idle]
  S --> T

```
