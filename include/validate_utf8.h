/*
 * Copyright 2011 self.disconnect
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This is the state transition table for validating UTF-8 input.
 *
 * We start in the initial state (s0) and expect to be in that state at the end
 * of the input sequence. If we transition to the error state (e) or end in a
 * state other than end state (s0), we consider that input sequence to be
 * invalid. In an attempt to make the table a little easier to understand,
 * states t1, t2, and t3 are used to signify the various tail states. The
 * number following the t is the number of remaining tail inputs remaining. A
 * tail accepts input in the range of %x80 - %xBF. With valid input, t3
 * transitions to t2, t2 transitions to t1, and t1 transition back to s0.
 *
 * States s1 through s4 are used to handle the more complex intermediary
 * transitions.
 *
 * Here is the ABNF from which the table was derived (see RFC 3629):
 *
 *   UTF8-octets = *( UTF8-char )
 *   UTF8-char   = UTF8-1 / UTF8-2 / UTF8-3 / UTF8-4
 *   UTF8-1      = %x00-7F
 *   UTF8-2      = %xC2-DF UTF8-tail
 *   UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8 - tail ) /
 *                 %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8 - tail )
 *   UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8 - tail ) /
 *                 %xF4 %x80-8F 2( UTF8-tail )
 *   UTF8-tail   = %x80-BF
 *
 * start           end
 * state  input   state
 * --------------------
 *  s0 : %x00-7F => s0
 *  s0 : %x80-C1 => e
 *  s0 : %xC2-DF => t1
 *  s0 : %xE0    => s1
 *  s0 : %xE1-EC => t2
 *  s0 : %xED    =>