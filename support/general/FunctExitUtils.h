/**
 * Copyright (c) 2020 Jay Logue
 * All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 *
 */

#ifndef FUNCTEXITUTILS_H
#define FUNCTEXITUTILS_H

#ifndef FUNCT_EXIT_SUCCESS
#define FUNCT_EXIT_SUCCESS 0
#endif

#ifndef FUNCT_EXIT_IS_SUCCESS
#define FUNCT_EXIT_IS_SUCCESS(condition) ((condition) == FUNCT_EXIT_SUCCESS)
#endif

#ifndef FUNCT_EXIT_LABEL
#define FUNCT_EXIT_LABEL exit
#endif


/**
 *  SuccessOrExit(status)
 *
 *  Checks if \c status matches a success value (generally 0) and if not, performs
 *  a local goto to an exit label (generally \c exit).
 *
 *  Example Usage:
 *
 *  @code
 *  int TrySomeThings()
 *  {
 *      int res;
 *
 *      res = TrySomething();
 *      SuccessOrExit(res);
 *
 *      res = TrySomethingElse();
 *      SuccessOrExit(res);
 *
 *  exit:
 *      return res;
 *  }
 *  @endcode
 *
 *  @param[in]  status      A scalar value to be tested for success.
 *
 */
#define SuccessOrExit(result)                               \
    do {                                                    \
        if (!FUNCT_EXIT_IS_SUCCESS(result))                 \
        {                                                   \
            goto FUNCT_EXIT_LABEL;                          \
        }                                                   \
    } while (0)


/**
 *  VerifyOrExit(condition, actions...)
 *
 *  Checks if the given \c condition evaluates to true.  If not, \c actions...
 *  is executed and a local goto is performed to an exit label
 *  (generally \c exit).
 *
 *  Example Usage:
 *
 *  @code
 *  int Make1KBuffer(const uint8_t *& buf)
 *  {
 *      int res = NO_ERROR;
 *
 *      buf = (uint8_t *)malloc(1024);
 *      VerifyOrExit(buf != NULL, res = ERROR_NO_MEMORY);
 *
 *      memset(buf, 0, 1024);
 *
 *  exit:
 *      return res;
 *  }
 *  @endcode
 *
 *  @param[in]  condition   A boolean expression to be evaluated.
 *  @param[in]  actions...  A block, or a sequence of expressions
 *                          separated by commas, to be executed if
 *                          condition evaluates to false.
 */
#define VerifyOrExit(condition, ...)                        \
    do {                                                    \
        if (!(condition))                                   \
        {                                                   \
            __VA_ARGS__;                                    \
            goto FUNCT_EXIT_LABEL;                          \
        }                                                   \
    } while (0)


/**
 *  ExitNow(actions...)
 *
 *  Unconditionally executes \c actions... and performs a local
 *  goto to an exit label (generally \c exit).
 *
 *  @note The use of this interface implies neither success nor
 *        failure for the overall exit status of the enclosing function
 *        body.
 *
 *  Example Usage:
 *
 *  @code
 *  int WaitForChar(char ch)
 *  {
 *      int res = NO_ERROR;
 *
 *      while (true)
 *      {
 *          int in = fgetc(STDIN);
 *          if (in == EOF)
 *              ExitNow(res = ERROR_UNEXPECTED_EOF);
 *          if (in == ch)
 *              break;
 *      }
 *
 *      printf("Got %c\n", ch);
 *
 *  exit:
 *      return err;
 *  }
 *  @endcode
 *
 *  @param[in]  actions...  A block, or a sequence of expressions
 *                          separated by commas, to be executed.
 *
 */
#define ExitNow(...)                                        \
    do {                                                    \
        __VA_ARGS__;                                        \
        goto FUNCT_EXIT_LABEL;                              \
    } while (0)



#endif // FUNCTEXITUTILS_H
