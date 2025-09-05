#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_SHARES 100
#define MAX_DEGREE 50
#define MAX_FILE_SIZE 10000
#define MAX_VALUE_LENGTH 50
#define MAX_KEY_LENGTH 20

// Structure to hold a point (x, y)
typedef struct {
    long long x;
    long long y;
} Point;

// Structure to hold share data
typedef struct {
    long long x;
    char value[MAX_VALUE_LENGTH];
    int base;
} Share;

// Structure to hold parsed JSON data
typedef struct {
    int n;
    int k;
    Share shares[MAX_SHARES];
    int share_count;
} ShareData;

// Convert digit character to integer value
int digitVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    return -1;
}

// Convert string in given base to decimal
long long convertToDecimal(const char *s, int base) {
    long long result = 0;
    int len = strlen(s);
    
    for (int i = 0; i < len; i++) {
        int d = digitVal(s[i]);
        if (d < 0 || d >= base) {
            fprintf(stderr, "Invalid digit '%c' for base %d\n", s[i], base);
            exit(1);
        }
        result = result * base + d;
    }
    return result;
}

// Skip whitespace characters
char* skipWhitespace(char* str) {
    while (*str && isspace(*str)) {
        str++;
    }
    return str;
}

// Find the next occurrence of a character, skipping strings
char* findChar(char* str, char target) {
    int inString = 0;
    char* current = str;
    
    while (*current) {
        if (*current == '"' && (current == str || *(current - 1) != '\\')) {
            inString = !inString;
        } else if (!inString && *current == target) {
            return current;
        }
        current++;
    }
    return NULL;
}

// Extract string value from JSON (removes quotes)
int extractStringValue(char* start, char* end, char* output, int maxLen) {
    // Skip opening quote
    if (*start == '"') start++;
    
    // Find closing quote
    char* closeQuote = strchr(start, '"');
    if (closeQuote && closeQuote < end) {
        end = closeQuote;
    }
    
    int len = end - start;
    if (len >= maxLen) len = maxLen - 1;
    
    strncpy(output, start, len);
    output[len] = '\0';
    return len;
}

// Extract integer value from JSON
int extractIntValue(char* start, char* end) {
    char buffer[20];
    int len = end - start;
    if (len >= 20) len = 19;
    
    strncpy(buffer, start, len);
    buffer[len] = '\0';
    
    return atoi(buffer);
}

// Dynamic JSON parser with better error handling
int parseJSONDynamic(const char *filename, ShareData *data) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file '%s'\n", filename);
        printf("Make sure the file exists and you have read permissions.\n");
        return 0;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize > MAX_FILE_SIZE) {
        printf("Error: File too large (max %d bytes)\n", MAX_FILE_SIZE);
        fclose(file);
        return 0;
    }
    
    // Allocate buffer and read file
    char *content = malloc(fileSize + 1);
    if (!content) {
        printf("Error: Memory allocation failed\n");
        fclose(file);
        return 0;
    }
    
    size_t bytesRead = fread(content, 1, fileSize, file);
    content[bytesRead] = '\0';
    fclose(file);
    
    printf("Successfully loaded JSON file (%ld bytes)\n", bytesRead);
    
    // Initialize data
    data->n = 0;
    data->k = 0;
    data->share_count = 0;
    
    // Parse n value
    char *nPos = strstr(content, "\"n\"");
    if (nPos) {
        char *colon = strchr(nPos, ':');
        if (colon) {
            colon = skipWhitespace(colon + 1);
            char *comma = findChar(colon, ',');
            char *brace = findChar(colon, '}');
            char *end = (comma && (!brace || comma < brace)) ? comma : brace;
            
            if (end) {
                data->n = extractIntValue(colon, end);
                printf("Parsed n = %d\n", data->n);
            }
        }
    }
    
    // Parse k value
    char *kPos = strstr(content, "\"k\"");
    if (kPos) {
        char *colon = strchr(kPos, ':');
        if (colon) {
            colon = skipWhitespace(colon + 1);
            char *comma = findChar(colon, ',');
            char *brace = findChar(colon, '}');
            char *end = (comma && (!brace || comma < brace)) ? comma : brace;
            
            if (end) {
                data->k = extractIntValue(colon, end);
                printf("Parsed k = %d\n", data->k);
            }
        }
    }
    
    // Parse keys/shares
    char *keysPos = strstr(content, "\"keys\"");
    if (!keysPos) {
        printf("Error: 'keys' section not found in JSON\n");
        free(content);
        return 0;
    }
    
    char *keysStart = strchr(keysPos, '{');
    if (!keysStart) {
        printf("Error: Invalid keys section format\n");
        free(content);
        return 0;
    }
    
    char *current = keysStart + 1;
    
    printf("\nParsing shares:\n");
    
    while (data->share_count < MAX_SHARES) {
        current = skipWhitespace(current);
        
        // Check for end of keys object
        if (*current == '}') break;
        
        // Skip comma if not first share
        if (*current == ',') {
            current = skipWhitespace(current + 1);
        }
        
        // Find key (x value)
        if (*current != '"') break;
        
        char *keyStart = current + 1;
        char *keyEnd = strchr(keyStart, '"');
        if (!keyEnd) break;
        
        char keyStr[MAX_KEY_LENGTH];
        extractStringValue(keyStart - 1, keyEnd + 1, keyStr, MAX_KEY_LENGTH);
        data->shares[data->share_count].x = atoll(keyStr);
        
        // Find the value object
        char *objectStart = strchr(keyEnd, '{');
        if (!objectStart) break;
        
        char *objectEnd = findChar(objectStart, '}');
        if (!objectEnd) break;
        
        // Parse base within this object
        char *basePos = strstr(objectStart, "\"base\"");
        if (basePos && basePos < objectEnd) {
            char *baseColon = strchr(basePos, ':');
            if (baseColon) {
                baseColon = skipWhitespace(baseColon + 1);
                if (*baseColon == '"') baseColon++;
                
                char *baseEnd = strpbrk(baseColon, "\",}");
                if (baseEnd) {
                    data->shares[data->share_count].base = extractIntValue(baseColon, baseEnd);
                }
            }
        }
        
        // Parse value within this object
        char *valuePos = strstr(objectStart, "\"value\"");
        if (valuePos && valuePos < objectEnd) {
            char *valueColon = strchr(valuePos, ':');
            if (valueColon) {
                valueColon = skipWhitespace(valueColon + 1);
                char *valueStart = strchr(valueColon, '"');
                if (valueStart) {
                    char *valueEnd = strchr(valueStart + 1, '"');
                    if (valueEnd) {
                        extractStringValue(valueStart, valueEnd + 1, 
                                         data->shares[data->share_count].value, 
                                         MAX_VALUE_LENGTH);
                    }
                }
            }
        }
        
        printf("  Share %d: x=%lld, base=%d, value=\"%s\"\n", 
               data->share_count + 1, 
               data->shares[data->share_count].x,
               data->shares[data->share_count].base,
               data->shares[data->share_count].value);
        
        data->share_count++;
        current = objectEnd + 1;
    }
    
    free(content);
    
    printf("Successfully parsed %d shares\n", data->share_count);
    
    // Validation
    if (data->n <= 0 || data->k <= 0) {
        printf("Error: Invalid n or k values\n");
        return 0;
    }
    
    if (data->share_count < data->k) {
        printf("Error: Not enough shares (%d) for reconstruction (need %d)\n", 
               data->share_count, data->k);
        return 0;
    }
    
    return 1;
}

// Multiply polynomial by linear factor (x - r)
void polyMul(double *poly, int degree, double r, double *result) {
    for (int i = 0; i <= degree + 1; i++) {
        result[i] = 0.0;
    }
    
    for (int i = 0; i <= degree; i++) {
        result[i] -= poly[i] * r;      
        result[i + 1] += poly[i];      
    }
}

// Lagrange interpolation to find polynomial coefficients
void lagrange(Point *points, int k, double *coeffs) {
    for (int i = 0; i < k; i++) {
        coeffs[i] = 0.0;
    }
    
    for (int i = 0; i < k; i++) {
        double xi = (double)points[i].x;
        double yi = (double)points[i].y;
        
        double basis[MAX_DEGREE];
        double temp[MAX_DEGREE];
        basis[0] = 1.0;
        for (int j = 1; j < MAX_DEGREE; j++) {
            basis[j] = 0.0;
        }
        
        int current_degree = 0;
        double denom = 1.0;
        
        for (int j = 0; j < k; j++) {
            if (j == i) continue;
            
            double xj = (double)points[j].x;
            polyMul(basis, current_degree, xj, temp);
            
            current_degree++;
            for (int d = 0; d <= current_degree; d++) {
                basis[d] = temp[d];
            }
            
            denom *= (xi - xj);
        }
        
        for (int d = 0; d <= current_degree; d++) {
            coeffs[d] += basis[d] * (yi / denom);
        }
    }
}

// Print polynomial in readable format
void printPolynomial(double *coeffs, int degree) {
    printf("P(x) = ");
    int first = 1;
    
    for (int i = 0; i <= degree; i++) {
        if (fabs(coeffs[i]) < 1e-10) continue;
        
        if (!first) {
            printf(" %s ", (coeffs[i] > 0) ? "+" : "-");
            printf("%.6f", fabs(coeffs[i]));
        } else {
            printf("%.6f", coeffs[i]);
            first = 0;
        }
        
        if (i == 1) printf("x");
        else if (i > 1) printf("x^%d", i);
    }
    printf("\n");
}

// Evaluate polynomial at given x
double evaluatePolynomial(double *coeffs, int degree, double x) {
    double result = 0.0;
    double xPower = 1.0;
    
    for (int i = 0; i <= degree; i++) {
        result += coeffs[i] * xPower;
        xPower *= x;
    }
    
    return result;
}

// Create a sample JSON file
void createSampleJSON(const char* filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Error creating sample file\n");
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "    \"keys\": {\n");
    fprintf(file, "        \"1\": {\n");
    fprintf(file, "            \"base\": \"10\",\n");
    fprintf(file, "            \"value\": \"4\"\n");
    fprintf(file, "        },\n");
    fprintf(file, "        \"2\": {\n");
    fprintf(file, "            \"base\": \"2\",\n");
    fprintf(file, "            \"value\": \"111\"\n");
    fprintf(file, "        },\n");
    fprintf(file, "        \"3\": {\n");
    fprintf(file, "            \"base\": \"10\",\n");
    fprintf(file, "            \"value\": \"12\"\n");
    fprintf(file, "        },\n");
    fprintf(file, "        \"6\": {\n");
    fprintf(file, "            \"base\": \"4\",\n");
    fprintf(file, "            \"value\": \"213\"\n");
    fprintf(file, "        }\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"n\": 4,\n");
    fprintf(file, "    \"k\": 3\n");
    fprintf(file, "}\n");
    
    fclose(file);
    printf("Sample JSON file '%s' created successfully!\n", filename);
}

int main() {
    printf("=== Shamir's Secret Sharing - Dynamic JSON Parser (C Version) ===\n\n");
    
    Point points[MAX_SHARES];
    int totalPoints = 0;
    int k = 0;
    
    printf("Choose input method:\n");
    printf("1. Use hardcoded example\n");
    printf("2. Read from JSON file\n");
    printf("3. Create sample JSON file\n");
    printf("Enter choice (1, 2, or 3): ");
    
    int choice;
    scanf("%d", &choice);
    
    if (choice == 3) {
        char filename[256];
        printf("Enter filename for sample JSON (e.g., sample.json): ");
        scanf("%s", filename);
        createSampleJSON(filename);
        return 0;
    }
    
    if (choice == 2) {
        char filename[256];
        printf("Enter JSON filename: ");
        scanf("%s", filename);
        
        ShareData data;
        if (!parseJSONDynamic(filename, &data)) {
            printf("Failed to parse JSON file. Try option 3 to create a sample file.\n");
            return 1;
        }
        
        k = data.k;
        
        printf("\n=== JSON Data Summary ===\n");
        printf("Total shares (n) = %d\n", data.n);
        printf("Threshold (k) = %d\n", data.k);
        printf("Loaded shares = %d\n", data.share_count);
        
        printf("\nConverting to decimal points:\n");
        for (int i = 0; i < data.share_count; i++) {
            long long x = data.shares[i].x;
            char *valueStr = data.shares[i].value;
            int base = data.shares[i].base;
            long long y = convertToDecimal(valueStr, base);
            
            points[totalPoints].x = x;
            points[totalPoints].y = y;
            totalPoints++;
            
            printf("  Point %d: x=%lld, value=\"%s\" (base %d) → y=%lld\n", 
                   i + 1, x, valueStr, base, y);
        }
    } else {
        // Hardcoded example
        k = 3;
        printf("\nUsing hardcoded example:\n");
        
        points[0].x = 1; points[0].y = convertToDecimal("4", 10);
        points[1].x = 2; points[1].y = convertToDecimal("111", 2);
        points[2].x = 3; points[2].y = convertToDecimal("12", 10);
        points[3].x = 6; points[3].y = convertToDecimal("213", 4);
        totalPoints = 4;
        
        printf("Points:\n");
        for (int i = 0; i < totalPoints; i++) {
            printf("  Point %d: (%lld, %lld)\n", i + 1, points[i].x, points[i].y);
        }
    }
    
    if (totalPoints < k) {
        printf("Error: Need at least %d points for reconstruction (have %d)\n", k, totalPoints);
        return 1;
    }
    
    printf("\n=== Lagrange Interpolation ===\n");
    printf("Using first %d points for polynomial reconstruction...\n", k);
    
    // Show selected points
    printf("Selected points for interpolation:\n");
    for (int i = 0; i < k; i++) {
        printf("  (%lld, %lld)\n", points[i].x, points[i].y);
    }
    
    // Perform interpolation using first k points
    double coeffs[MAX_DEGREE];
    lagrange(points, k, coeffs);
    
    printf("\nPolynomial coefficients (P(x) = a₀ + a₁x + a₂x² + ...):\n");
    for (int i = 0; i < k; i++) {
        printf("  a%d = %.10f\n", i, coeffs[i]);
    }
    
    printf("\nReconstructed polynomial:\n");
    printPolynomial(coeffs, k - 1);
    
    printf("\n=== Verification ===\n");
    printf("Testing reconstructed polynomial against all points:\n");
    int allMatch = 1;
    for (int i = 0; i < totalPoints; i++) {
        double val = evaluatePolynomial(coeffs, k - 1, (double)points[i].x);
        int matches = fabs(val - points[i].y) < 1e-9;
        
        printf("  P(%lld) = %.2f, expected %lld %s\n", 
               points[i].x, val, points[i].y, matches ? "✓" : "✗");
        
        if (!matches) allMatch = 0;
    }
    
    printf("\n=== Secret Recovery ===\n");
    double secret = coeffs[0]; // The constant term is the secret (P(0))
    printf("🔑 Secret (P(0)) = %.0f\n", secret);
    
    if (allMatch) {
        printf("\n✅ All points verified successfully!\n");
        printf("✅ Polynomial reconstruction completed!\n");
        printf("✅ Secret successfully recovered: %.0f\n", secret);
    } else {
        printf("\n⚠️  Warning: Some points don't match the reconstructed polynomial\n");
        printf("   This might indicate corrupted shares or insufficient threshold\n");
    }
    
    return 0;
}
