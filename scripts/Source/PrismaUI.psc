Scriptname PrismaUI Native Hidden
{Framework-native Prisma UI control. Views are addressed by a caller-chosen string name.}

bool Function CreateView(string asName, string asHtmlPath) global native
Function Push(string asName, string asFunction, string asJson) global native
Function Show(string asName) global native
Function Hide(string asName) global native
Function Destroy(string asName) global native
bool Function IsValid(string asName) global native

; Gives a live named view keyboard/mouse focus. Returns true when the request was accepted for a
; currently-live view. The actual focus change is queued onto the framework task path.
bool Function Focus(string asName, bool abPauseGame, bool abDisableFocusMenu) global native
Function Unfocus(string asName) global native
bool Function HasFocus(string asName) global native

; Marks this view for offscreen rendering so its live Prisma texture can be bound to geometry.
Function SetOffscreen(string asName, bool abOffscreen) global native

; Resolution of that render texture. Match the aspect ratio of the target quad, not necessarily the
; game window. Call before SetOffscreen(true) for the intended layout from the first offscreen frame;
; calling it later resizes the live view. Ignored when either value is <= 0.
Function SetOffscreenSize(string asName, int aiWidth, int aiHeight) global native

; Bind by exact BSGeometry node name, e.g. "Screen:0".
; The framework retries briefly while the reference 3D and view texture come online.
bool Function BindToObject(string asName, ObjectReference akRef, string asNode, bool abFirstPerson) global native

; Bind by a case-insensitive substring of the current diffuse texture path. This is useful when
; replacers rename screen nodes but retain a recognizable material, e.g. "terminalscreen" or "tv".
bool Function BindToTexture(string asName, ObjectReference akRef, string asTextureSubstring, bool abFirstPerson) global native

Function Unbind(string asName) global native
