CC = cc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS =

SRC = main.c lexer.c parser.c parser_util.c ast.c types.c checker.c codegen.c print_ast.c
OBJ = $(SRC:.c=.o)
DEPS = $(SRC:.c=.d)
BIN = bin
TARGET = $(BIN)/w0

.PHONY: all clean test test-verbose test-valid test-errors test-help format

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(BIN):
	mkdir -p $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.d: %.c
	$(CC) -MM $< -MF $@

clean:
	rm -f $(OBJ) $(DEPS)
	rm -rf $(BIN)

test: $(TARGET)
	@echo "\033[1;34m=== Running W0 Test Suite ===\033[0m"
	@echo ""
	@echo "\033[1;36m=== Valid Programs (should pass) ===\033[0m"
	@passed=0; failed=0; \
	for f in test/*.w; do \
		case "$$f" in test/error_*) continue;; esac; \
		printf "\033[1m%-35s\033[0m" "$$(basename "$$f"):"; \
		if $(TARGET) --check "$$f" >/dev/null 2>&1; then \
			echo " \033[1;32m✓ PASS\033[0m"; \
			passed=$$((passed + 1)); \
		else \
			echo " \033[1;31m✗ FAIL\033[0m"; \
			$(TARGET) --check "$$f" 2>&1 | sed 's/^/  \033[31m/' | sed 's/$$/\033[0m/'; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "\033[1;36m=== Error Cases (should fail with specific errors) ===\033[0m"; \
	error_passed=0; error_failed=0; \
	for f in test/error_*.w; do \
		printf "\033[1m%-35s\033[0m" "$$(basename "$$f"):"; \
		expected_error=$$(grep "// Expected error:" "$$f" | sed 's|.*// Expected error: *||' | head -1); \
		if output=$$($(TARGET) --check "$$f" 2>&1); then \
			echo " \033[1;31m✗ FAIL (should have failed)\033[0m"; \
			echo "  \033[33mExpected: $$expected_error\033[0m"; \
			echo "  \033[33mBut test passed unexpectedly\033[0m"; \
			error_failed=$$((error_failed + 1)); \
		else \
			actual_error=$$(echo "$$output" | grep "Error:" | head -1 | sed 's|.*Error: *||'); \
			if echo "$$output" | grep -q "Error:"; then \
				if echo "$$actual_error" | grep -qF "$$expected_error"; then \
					echo " \033[1;32m✓ PASS (correct error)\033[0m"; \
					echo "  \033[90m$$actual_error\033[0m"; \
					error_passed=$$((error_passed + 1)); \
				else \
					echo " \033[1;31m✗ FAIL (wrong error message)\033[0m"; \
					echo "  \033[33mExpected: $$expected_error\033[0m"; \
					echo "  \033[31mActual:   $$actual_error\033[0m"; \
					error_failed=$$((error_failed + 1)); \
				fi; \
			else \
				echo " \033[1;31m✗ FAIL (no error message)\033[0m"; \
				echo "  \033[33mExpected: $$expected_error\033[0m"; \
				echo "$$output" | sed 's/^/  \033[31m/' | sed 's/$$/\033[0m/'; \
				error_failed=$$((error_failed + 1)); \
			fi; \
		fi; \
	done; \
	echo ""; \
	echo "\033[1;34m=== Test Summary ===\033[0m"; \
	total_passed=$$((passed + error_passed)); \
	total_failed=$$((failed + error_failed)); \
	total_tests=$$((total_passed + total_failed)); \
	echo "\033[1;32mValid Programs: $$passed/$$((passed + failed)) passed\033[0m"; \
	echo "\033[1;32mError Cases:    $$error_passed/$$((error_passed + error_failed)) passed\033[0m"; \
	echo "\033[1;36mTotal:          $$total_passed/$$total_tests tests passed\033[0m"; \
	if [ $$total_failed -eq 0 ]; then \
		echo "\033[1;32m🎉 All tests passed!\033[0m"; \
		exit 0; \
	else \
		echo "\033[1;31m❌ $$total_failed test(s) failed\033[0m"; \
		exit 1; \
	fi

test-verbose: $(TARGET)
	@echo "\033[1;34m=== Running W0 Test Suite (Verbose Mode) ===\033[0m"
	@echo ""
	@echo "\033[1;36m=== Valid Programs ===\033[0m"
	@for f in test/*.w; do \
		case "$$f" in test/error_*) continue;; esac; \
		echo "\033[1;33m--- Testing $$f ---\033[0m"; \
		if $(TARGET) --check "$$f" 2>&1; then \
			echo "\033[1;32m✓ PASS\033[0m"; \
		else \
			echo "\033[1;31m✗ FAIL\033[0m"; \
		fi; \
		echo ""; \
	done; \
	echo "\033[1;36m=== Error Cases ===\033[0m"; \
	for f in test/error_*.w; do \
		echo "\033[1;33m--- Testing $$f ---\033[0m"; \
		echo "\033[90mExpected to fail with specific error\033[0m"; \
		if output=$$($(TARGET) --check "$$f" 2>&1); then \
			echo "\033[1;31m✗ FAIL (should have failed but passed)\033[0m"; \
		else \
			echo "$$output"; \
			if echo "$$output" | grep -q "Error:"; then \
				echo "\033[1;32m✓ PASS (correctly failed with error)\033[0m"; \
			else \
				echo "\033[1;31m✗ FAIL (failed but no error message)\033[0m"; \
			fi; \
		fi; \
		echo ""; \
	done

test-valid: $(TARGET)
	@echo "\033[1;36m=== Testing Valid Programs Only ===\033[0m"
	@passed=0; failed=0; \
	for f in test/*.w; do \
		case "$$f" in test/error_*) continue;; esac; \
		printf "\033[1m%-35s\033[0m" "$$(basename "$$f"):"; \
		if $(TARGET) --check "$$f" >/dev/null 2>&1; then \
			echo " \033[1;32m✓ PASS\033[0m"; \
			passed=$$((passed + 1)); \
		else \
			echo " \033[1;31m✗ FAIL\033[0m"; \
			$(TARGET) --check "$$f" 2>&1 | sed 's/^/  \033[31m/' | sed 's/$$/\033[0m/'; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "\033[1;32mResult: $$passed/$$((passed + failed)) valid programs passed\033[0m"

test-errors: $(TARGET)
	@echo "\033[1;36m=== Testing Error Cases Only ===\033[0m"
	@error_passed=0; error_failed=0; \
	for f in test/error_*.w; do \
		printf "\033[1m%-35s\033[0m" "$$(basename "$$f"):"; \
		expected_error=$$(grep "// Expected error:" "$$f" | sed 's|.*// Expected error: *||' | head -1); \
		if output=$$($(TARGET) --check "$$f" 2>&1); then \
			echo " \033[1;31m✗ FAIL (should have failed)\033[0m"; \
			echo "  \033[33mExpected: $$expected_error\033[0m"; \
			echo "  \033[33mBut test passed unexpectedly\033[0m"; \
			error_failed=$$((error_failed + 1)); \
		else \
			actual_error=$$(echo "$$output" | grep "Error:" | head -1 | sed 's|.*Error: *||'); \
			if echo "$$output" | grep -q "Error:"; then \
				if echo "$$actual_error" | grep -qF "$$expected_error"; then \
					echo " \033[1;32m✓ PASS (correct error)\033[0m"; \
					echo "  \033[90m$$actual_error\033[0m"; \
					error_passed=$$((error_passed + 1)); \
				else \
					echo " \033[1;31m✗ FAIL (wrong error message)\033[0m"; \
					echo "  \033[33mExpected: $$expected_error\033[0m"; \
					echo "  \033[31mActual:   $$actual_error\033[0m"; \
					error_failed=$$((error_failed + 1)); \
				fi; \
			else \
				echo " \033[1;31m✗ FAIL (no error message)\033[0m"; \
				echo "  \033[33mExpected: $$expected_error\033[0m"; \
				echo "$$output" | sed 's/^/  \033[31m/' | sed 's/$$/\033[0m/'; \
				error_failed=$$((error_failed + 1)); \
			fi; \
		fi; \
	done; \
	echo "\033[1;32mResult: $$error_passed/$$((error_passed + error_failed)) error cases passed\033[0m"

test-help:
	@echo "\033[1;34mW0 Test Suite Commands:\033[0m"
	@echo ""
	@echo "\033[1;32m  make test\033[0m        - Run all tests (valid programs + error cases)"
	@echo "\033[1;32m  make test-valid\033[0m  - Run only valid program tests"
	@echo "\033[1;32m  make test-errors\033[0m - Run only error case tests"
	@echo "\033[1;32m  make test-verbose\033[0m- Run all tests with detailed output"
	@echo "\033[1;32m  make test-help\033[0m   - Show this help message"
	@echo ""
	@echo "\033[1;36mTest Organization:\033[0m"
	@echo "  \033[33mValid tests:\033[0m test/*.w (excluding error_*.w)"
	@echo "  \033[33mError tests:\033[0m test/error_*.w (should fail with specific errors)"
	@echo ""
	@echo "\033[1;36mError Test Format:\033[0m"
	@echo "  Each error test file must include: \033[33m// Expected error: <message>\033[0m"
	@echo "  The actual error must contain the expected error text"
	@echo ""
	@echo "\033[1;36mColor Legend:\033[0m"
	@echo "  \033[1;32m✓ PASS (correct error)\033[0m - Error test failed with expected message"
	@echo "  \033[1;31m✗ FAIL (wrong error message)\033[0m - Error test failed with wrong message"
	@echo "  \033[1;31m✗ FAIL (should have failed)\033[0m - Error test passed unexpectedly"
	@echo "  \033[90mGray text\033[0m - Actual error messages from successful error tests"

format:
	clang-format -i *.c *.h

-include $(DEPS)