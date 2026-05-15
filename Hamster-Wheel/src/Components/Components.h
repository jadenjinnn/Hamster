#ifndef UIPROTO_COMPONENTS_H
#define UIPROTO_COMPONENTS_H

#include <imgui.h>

inline constexpr float kLabelCol  = 90.0f;
inline constexpr float kScrollGap = 8.0f;

void SectionSeparator(float spacingBefore = 10.0f, float spacingAfter = 10.0f);
void DrawGradientBar(ImVec2 p0, ImVec2 p1, ImU32 left, ImU32 right, float rounding);
void SectionHeader(const char *label);
bool HButton(const char *label, float width = 0.0f);
bool HToolbarButton(const char *label);
void HCombo(const char *label, const char *id, int *val, const char *items[], int count);
bool HBeginCombo(const char *id, const char *preview, float width = -kScrollGap);
void HEndCombo();
bool HComboItem(const char *label, bool selected);
bool HBeginStyledPopup(const char *id);
void HEndStyledPopup();
void HDragFloat(const char *label, const char *id, float *val, float speed, float lo, float hi);
void HCheckbox(const char *label, const char *id, bool *val);
void AxisDotInput(const char *letter, const char *inputId, float *val, ImU32 dotColor, float totalW);

#endif // UIPROTO_COMPONENTS_H
