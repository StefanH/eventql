/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#ifndef _STX_CURRENCY_H
#define _STX_CURRENCY_H
#include "eventql/util/stdtypes.h"
#include "eventql/util/Currency.pb.h"

namespace util {

Currency currencyFromString(const String& string);
String currencyToString(Currency lang);

struct Money {
  Money(uint64_t cents, Currency currency);
  const uint64_t cents;
  const Currency currency;
};

class CurrencyConverter {
public:
  // <FROM, TO, RATE>
  typedef Tuple<Currency, Currency, double> ConversionRate;
  typedef Vector<ConversionRate> ConversionTable;

  CurrencyConverter(const ConversionTable& conversion_table);

  Money convert(Money amount, Currency target_currency) const;

protected:
  const ConversionTable conv_table_;
};


} // namespace util

#endif
