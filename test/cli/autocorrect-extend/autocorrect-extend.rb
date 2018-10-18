# typed: strict

# Don't extend T::Helpers for this one, because the class wasn't defined here.
def foo; end

# Should only add one extend
class SuggestSigAndExtend
  def foo; end

  def bar; end
end

# This doesn't work right now, for both the injected 'sig' and 'extend'.
class SigAndExtendOneLine; def foo; end; end

class DontSuggestExtend
  extend T::Helpers
  def foo; end
end

class Parent; end
class Child < Parent
  def foo; end
end

def project(x); x; end

class MethodBetweenExtendAndSig
  project :sorbet
  def foo; end
end

class MethodOnSelf
  def self.foo; end
end
