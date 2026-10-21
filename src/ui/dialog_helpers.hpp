/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_DIALOG_HELPERS_HPP_
#define CHESSBOARD_UI_DIALOG_HELPERS_HPP_

class QDialog;
class QDialogButtonBox;

namespace chess {
// A lone Close button wired so both accepted and rejected end the dialog via
// owner->accept() (a Close button emits rejected).
QDialogButtonBox* MakeCloseButtonBox(QDialog* owner);

// Widens `dialog` so its title isn't truncated: some window managers (e.g.
// GNOME/Wayland) size the title area from the content width. Call after the
// layout has been sized.
void EnsureTitleFits(QDialog* dialog);
}  // namespace chess

#endif  // CHESSBOARD_UI_DIALOG_HELPERS_HPP_
