BUILDDIR=build
CXXFLAGS+=-Wall -Wextra

tests: $(BUILDDIR)/refcount_struct_test

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/%_test: %_test.cc *.h $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Wno-unused-value -o $@ $<
	./$@

clean:
	rm -rf $(BUILDDIR)

.PHONY: tests clean
