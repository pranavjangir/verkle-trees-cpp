import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Scanner;

public class EtherscanClient {
    private final String API_KEY;
    private final String API_BASE_URL = "https://api.etherscan.io/api?";

    public EtherscanClient(String apiKey) {
        this.API_KEY = apiKey;
    }

    public String getBlockByNumber(String blockTag, Boolean inDepthTransactions) throws IOException {
        String url = API_BASE_URL + "module=proxy&action=eth_getBlockByNumber&tag=" + blockTag + "&boolean="+inDepthTransactions+"&apikey=" + API_KEY;

        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        try (InputStreamReader reader = new InputStreamReader(conn.getInputStream())) {
            Scanner scanner = new Scanner(reader);
            return scanner.useDelimiter("\\Z").next();
        }
    }

    public String getTransactionReceipt(String txHash) throws IOException {
        String url = API_BASE_URL + "module=proxy&action=eth_getTransactionReceipt&txhash=" + txHash + "&apikey=" + API_KEY;

        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        try (InputStreamReader reader = new InputStreamReader(conn.getInputStream())) {
            Scanner scanner = new Scanner(reader);
            return scanner.useDelimiter("\\Z").next();
        }
    }

    public String getLatestBlockNumber() throws IOException{
        String blockNumber=null;
        try {
            // Set the URL for the Etherscan API endpoint
            URL url = new URL("https://api.etherscan.io/api?module=proxy&action=eth_blockNumber");

            // Open a connection to the URL and make the GET request
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");

            // Read the response from the API and parse the block number
            BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
            String response = reader.readLine();
            blockNumber = (new JSONObject(response)).getString("result");

            // Print the block number to the console
            System.out.println("Latest block number: " + blockNumber);
        } catch (Exception e) {
            // Handle any exceptions that may occur
            e.printStackTrace();
        }
        return blockNumber;
    }
}
