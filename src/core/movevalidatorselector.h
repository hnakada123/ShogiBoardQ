#ifndef MOVEVALIDATORSELECTOR_H
#define MOVEVALIDATORSELECTOR_H

#include <type_traits>

#include "fastmovevalidator.h"
#include "movevalidator.h"

namespace MoveValidatorSelector {

// 機能フラグ: true = FastMoveValidator, false = MoveValidator
inline constexpr bool kUseFastMoveValidator = true;

using ActiveMoveValidator = std::conditional_t<kUseFastMoveValidator, FastMoveValidator, MoveValidator>;

} // namespace MoveValidatorSelector

#endif // MOVEVALIDATORSELECTOR_H
