#include "regionorder.h"

#include "dhlrc_core.h"

#include <QRegularExpression>
#include <algorithm>

namespace
{
/* True when `text` at `pos` starts a run of digits. */
bool
isDigit (QChar c)
{
  return c >= QLatin1Char ('0') && c <= QLatin1Char ('9');
}

/* Compares two digit runs by value without converting them: the longer run
 * (leading zeroes skipped) is the larger one, equal-length runs compare as
 * text. Runs of hundreds of digits therefore work too. */
int
compareDigits (const QString &left, qsizetype leftPos, qsizetype leftEnd,
               const QString &right, qsizetype rightPos, qsizetype rightEnd)
{
  while (leftPos < leftEnd && left.at (leftPos) == QLatin1Char ('0'))
    leftPos++;
  while (rightPos < rightEnd && right.at (rightPos) == QLatin1Char ('0'))
    rightPos++;
  const qsizetype leftLength = leftEnd - leftPos;
  const qsizetype rightLength = rightEnd - rightPos;
  if (leftLength != rightLength)
    return leftLength < rightLength ? -1 : 1;
  for (qsizetype i = 0; i < leftLength; i++)
    {
      const QChar l = left.at (leftPos + i);
      const QChar r = right.at (rightPos + i);
      if (l != r)
        return l < r ? -1 : 1;
    }
  return 0;
}

/* Turns one wildcard pattern into an anchored regular expression. Writing the
 * conversion here keeps the semantics to `*` (any text) and `?` (one
 * character), which is all the rule needs, and works on every Qt version. */
QRegularExpression
patternExpression (const QString &pattern)
{
  QString expression = QStringLiteral ("\\A");
  for (const QChar c : pattern)
    {
      if (c == QLatin1Char ('*'))
        expression += QStringLiteral (".*");
      else if (c == QLatin1Char ('?'))
        expression += QLatin1Char ('.');
      else
        expression += QRegularExpression::escape (QString (c));
    }
  expression += QStringLiteral ("\\z");
  QRegularExpression regex (expression);
  regex.setPatternOptions (QRegularExpression::CaseInsensitiveOption);
  return regex;
}

/* The wildcard patterns of the custom rule, in the order they were written.
 * Both the comma and the semicolon separate patterns, so a user can pick
 * whichever reads better. */
QStringList
rulePatterns (const QString &rule)
{
  QStringList patterns;
  for (const QString &part :
       rule.split (QRegularExpression (QStringLiteral ("[,;]"))))
    {
      const QString pattern = part.trimmed ();
      if (!pattern.isEmpty ())
        patterns.append (pattern);
    }
  return patterns;
}
}

int
dh::naturalCompare (const QString &left, const QString &right)
{
  qsizetype leftPos = 0;
  qsizetype rightPos = 0;
  while (leftPos < left.size () && rightPos < right.size ())
    {
      const QChar l = left.at (leftPos);
      const QChar r = right.at (rightPos);
      if (isDigit (l) && isDigit (r))
        {
          qsizetype leftEnd = leftPos;
          while (leftEnd < left.size () && isDigit (left.at (leftEnd)))
            leftEnd++;
          qsizetype rightEnd = rightPos;
          while (rightEnd < right.size () && isDigit (right.at (rightEnd)))
            rightEnd++;
          const int digits = compareDigits (left, leftPos, leftEnd, right,
                                            rightPos, rightEnd);
          if (digits != 0)
            return digits;
          leftPos = leftEnd;
          rightPos = rightEnd;
          continue;
        }
      const QChar leftFolded = l.toCaseFolded ();
      const QChar rightFolded = r.toCaseFolded ();
      if (leftFolded != rightFolded)
        return leftFolded < rightFolded ? -1 : 1;
      leftPos++;
      rightPos++;
    }
  /* One name is the beginning of the other: the shorter one comes first, and
   * the same text with a different case is equal. */
  if (leftPos < left.size ())
    return 1;
  if (rightPos < right.size ())
    return -1;
  return 0;
}

QList<int>
dh::regionLoadingOrder (const QStringList &names, int order,
                        const QString &rule)
{
  QList<int> indexes;
  indexes.reserve (names.size ());
  for (qsizetype i = 0; i < names.size (); i++)
    indexes.append (static_cast<int> (i));

  if (order == DHLRC_REGION_ORDER_FILE)
    return indexes;

  /* A pattern list with a bad expression would silently sort nothing useful,
   * so a pattern that does not compile is skipped instead of matched. */
  QList<QRegularExpression> patterns;
  if (order == DHLRC_REGION_ORDER_CUSTOM)
    {
      for (const QString &pattern : rulePatterns (rule))
        {
          const QRegularExpression expression = patternExpression (pattern);
          if (expression.isValid ())
            patterns.append (expression);
        }
    }

  auto group = [&patterns] (const QString &name)
    {
      for (qsizetype i = 0; i < patterns.size (); i++)
        {
          if (patterns.at (i).match (name).hasMatch ())
            return i;
        }
      return patterns.size ();
    };

  std::stable_sort (indexes.begin (), indexes.end (),
                    [&names, &group] (int left, int right)
                      {
                        const QString &leftName = names.at (left);
                        const QString &rightName = names.at (right);
                        const qsizetype leftGroup = group (leftName);
                        const qsizetype rightGroup = group (rightName);
                        if (leftGroup != rightGroup)
                          return leftGroup < rightGroup;
                        const int natural
                            = dh::naturalCompare (leftName, rightName);
                        if (natural != 0)
                          return natural < 0;
                        return left < right;
                      });
  return indexes;
}
