void *malloc(unsigned long);
void free(void *ptr);
int printf(const char *, ...);

int is_prime(int n);
int fibonacci(int n);
int factorial(int n);
void swap(int *a, int *b);
int partition(int arr[], int low, int high);
void quick_sort(int arr[], int low, int high);
int binary_search(int arr[], int size, int target);

typedef struct Node {
  int data;
  struct Node *next;
} Node;

Node *create_node(int data);
void list_insert(Node **head, int data);
void list_print(Node *head);
void list_free(Node *head);

int main(void) {
  printf("Is 7919 prime? %s\n", is_prime(7919) ? "Yes" : "No");
  printf("Fibonacci(15) = %d, it should be 610\n", fibonacci(15));

  int numbers[8] = {7, 2, 5, 3, 11, 1, 8, 9};
  int size = 8;
  printf("Before sort: ");
  for (int i = 0; i < size; i++) {
    printf("[%d] = %d ", i, numbers[i]);
  }
  printf("\n");
  quick_sort(numbers, 0, size - 1);
  printf("Sorted array: ");
  for (int i = 0; i < size; i++)
    printf("[%d] = %d ", i, numbers[i]);

  printf("\n");
  printf("Found 5 at index: %d\n", binary_search(numbers, size, 5));
  printf("Found 9 at index: %d\n", binary_search(numbers, size, 9));

  Node *mylist = (void *)0;
  for (int i = 0; i < 5; i++) {
    list_insert(&mylist, i * 10);
  }
  printf("Linked list contents: ");
  list_print(mylist);
  list_free(mylist);

  return 0;
}

int is_prime(int n) {
  if (n <= 1)
    return 0;
  if (n <= 3)
    return 1;
  if (n % 2 == 0 || n % 3 == 0)
    return 0;

  for (int i = 5; i * i <= n; i += 6) {
    if (n % i == 0 || n % (i + 2) == 0) {
      return 0;
    }
  }
  return 1;
}

int fibonacci(int n) {
  if (n <= 1)
    return n;
  int a = 0, b = 1, temp;
  for (int i = 2; i <= n; i++) {
    temp = a + b;
    a = b;
    b = temp;
  }
  return b;
}

int factorial(int n) {
  int result = 1;
  for (int i = 2; i <= n; i++) {
    result *= i;
  }
  return result;
}

void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

int partition(int arr[], int low, int high) {
  int pivot = arr[high];
  int i = low - 1;
  for (int j = low; j < high; j++) {
    if (arr[j] < pivot) {
      i++;
      swap(&arr[i], &arr[j]);
    }
  }
  swap(&arr[i + 1], &arr[high]);
  return i + 1;
}

void quick_sort(int arr[], int low, int high) {
  if (low < high) {
    int pi = partition(arr, low, high);
    quick_sort(arr, low, pi - 1);
    quick_sort(arr, pi + 1, high);
  }
}

int binary_search(int arr[], int size, int target) {
  int low = 0, high = size - 1;
  while (low <= high) {
    int mid = low + (high - low) / 2;
    if (arr[mid] == target)
      return mid;
    if (arr[mid] < target) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return -1;
}

Node *create_node(int data) {
  Node *new_node = malloc(16);
  if (new_node) {
    new_node->data = data;
    new_node->next = (void *)0;
  }
  return new_node;
}

void list_insert(Node **head, int data) {
  Node *new_node = create_node(data);
  if (new_node) {
    new_node->next = *head;
    *head = new_node;
  }
}

void list_print(Node *head) {
  while (head) {
    printf("%d ", head->data);
    head = head->next;
  }
  printf("\n");
}

void list_free(Node *head) {
  while (head) {
    Node *temp = head;
    head = head->next;
    free(temp);
  }
}
