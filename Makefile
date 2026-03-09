NAME		= webserv

CXX			= c++
CXXFLAGS	= -std=c++20 -Wall -Wextra -Werror -Wpedantic
INCLUDES	= -I includes

SRCS_DIR	= srcs
SRCS_DIRS	= $(SRCS_DIR) $(SRCS_DIR)/Webserv
SRCS		= $(foreach dir,$(SRCS_DIRS),$(wildcard $(dir)/*.cpp))

OBJS_DIR	= objs
OBJS		= $(patsubst $(SRCS_DIR)/%.cpp,$(OBJS_DIR)/%.o,$(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -o $(NAME)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJS_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

debug: CXXFLAGS += -DDEBUG -g
debug: re

.PHONY: all clean fclean re debug
